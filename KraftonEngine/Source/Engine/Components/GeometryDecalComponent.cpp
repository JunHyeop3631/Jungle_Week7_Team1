#include "Components/GeometryDecalComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/World.h"
#include "Collision/WorldPrimitivePickingBVH.h"
#include "Collision/MeshTrianglePickingBVH.h"
#include "Mesh/StaticMesh.h"
#include "Object/ObjectFactory.h"
#include "Materials/MaterialInterface.h"
#include "Resource/ResourceManager.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Render/Proxy/GeometryDecalSceneProxy.h"
#include "Render/Pipeline/RenderBus.h"
#include "Serialization/Archive.h"
#include "Collision/RayUtils.h"
#include "Engine/Runtime/Engine.h"
#include "Render/Resource/MeshBufferManager.h"
#include <cstring>
#include <algorithm>
#include <cmath>

IMPLEMENT_CLASS(UGeometryDecalComponent, UMeshComponent)

namespace
{
	constexpr const char* GeometryDecalTextureMaterialPrefix = "Texture:";

	bool IsGeometryDecalTextureMaterialPath(const FString& Path)
	{
		return Path.rfind(GeometryDecalTextureMaterialPrefix, 0) == 0;
	}

	FString GetGeometryDecalTextureNameFromMaterialPath(const FString& Path)
	{
		return IsGeometryDecalTextureMaterialPath(Path)
			? Path.substr(std::strlen(GeometryDecalTextureMaterialPrefix))
			: FString();
	}

	FString MakeGeometryDecalTextureMaterialPath(const FName& TextureName)
	{
		return FString(GeometryDecalTextureMaterialPrefix) + TextureName.ToString();
	}
}

UGeometryDecalComponent::UGeometryDecalComponent()
{
	// 초기 Extent (볼륨 크기)
	LocalExtents = FVector(0.5f, 0.5f, 0.5f);
}

UGeometryDecalComponent::~UGeometryDecalComponent()
{
	ClearDecalMeshInternal(false);
}

FMatrix UGeometryDecalComponent::GetTransformIncludingDecalSize() const
{
	return FMatrix::MakeScaleMatrix(DecalSize) * GetWorldMatrix();
}

void UGeometryDecalComponent::SetDecalSize(const FVector& InSize)
{
	DecalSize = InSize;

	// Keep volume/bounds update behavior aligned with UDecalComponent.
	MarkProxyDirty(EDirtyFlag::Transform);
	MarkWorldBoundsDirty();

	if (UWorld* World = GetWorld())
	{
		GenerateDecalMesh(*World);
	}
}

void UGeometryDecalComponent::SetMaterial(UMaterialInterface* InMaterial)
{
	DecalMaterial = InMaterial;
	DecalTextureName = FName();
	DecalTexture = nullptr;
	DecalMaterialSlot.Path = (DecalMaterial != nullptr) ? DecalMaterial->GetAssetPathFileName() : "None";
	MarkProxyDirty(EDirtyFlag::Material);
}

void UGeometryDecalComponent::SetDecalTexture(const FName& TextureName)
{
	DecalMaterial = nullptr;
	DecalTextureName = TextureName;
	DecalTexture = FResourceManager::Get().FindTexture(TextureName);
	DecalMaterialSlot.Path = DecalTexture ? MakeGeometryDecalTextureMaterialPath(TextureName) : "None";
	MarkProxyDirty(EDirtyFlag::Material);
}

// ==============================================================================
// Sutherland-Hodgman Clipping Core (FVertexPNCT 적용)
// ==============================================================================
FVertexPNCT UGeometryDecalComponent::InterpolateVertex(const FVertexPNCT& V1, const FVertexPNCT& V2, float t)
{
	FVertexPNCT Out;
	Out.Position = V1.Position + (V2.Position - V1.Position) * t;

	Out.Normal = V1.Normal + (V2.Normal - V1.Normal) * t;
	float nLenSq = Out.Normal.Dot(Out.Normal);
	if (nLenSq > 0.00001f) Out.Normal = Out.Normal * (1.0f / std::sqrt(nLenSq));
	else Out.Normal = FVector(0, 0, 1);

	FVector TangentV1(V1.Tangent.X, V1.Tangent.Y, V1.Tangent.Z);
	FVector TangentV2(V2.Tangent.X, V2.Tangent.Y, V2.Tangent.Z);
	FVector TangentOut = TangentV1 + (TangentV2 - TangentV1) * t;

	float tLenSq = TangentOut.Dot(TangentOut);
	if (tLenSq > 0.00001f) TangentOut = TangentOut * (1.0f / std::sqrt(tLenSq));
	else TangentOut = TangentV1;

	Out.Tangent = FVector4(TangentOut.X, TangentOut.Y, TangentOut.Z, V1.Tangent.W);
	Out.Color = V1.Color + (V2.Color - V1.Color) * t;
	Out.UV = V1.UV + (V2.UV - V1.UV) * t;
	return Out;
}

void UGeometryDecalComponent::ClipPolygonAgainstPlane(const FClipPlane& Plane, const TArray<FVertexPNCT>& InPoly, TArray<FVertexPNCT>& OutPoly)
{
	OutPoly.clear();
	if (InPoly.empty()) return;

	// 평면 방정식: N dot P + D = 0 (결과가 <= 0 이면 안쪽, > 0 이면 잘려나감)
	// Vector.h의 Dot 함수 활용
	auto GetDistance = [&](const FVector& P) { return Plane.Normal.Dot(P) + Plane.D; };

	FVertexPNCT PrevVert = InPoly.back();
	float PrevDist = GetDistance(PrevVert.Position);

	for (const FVertexPNCT& CurrVert : InPoly)
	{
		float CurrDist = GetDistance(CurrVert.Position);

		// 선분이 평면을 교차하는 경우 교차점(Intersection) 생성
		if ((CurrDist > 0.0f && PrevDist <= 0.0f) || (CurrDist <= 0.0f && PrevDist > 0.0f))
		{
			OutPoly.push_back(InterpolateVertex(PrevVert, CurrVert, PrevDist / (PrevDist - CurrDist)));
		}

		// 현재 점이 평면 안쪽이면 추가
		if (CurrDist <= 0.0f)
		{
			OutPoly.push_back(CurrVert);
		}

		PrevVert = CurrVert;
		PrevDist = CurrDist;
	}
}

void UGeometryDecalComponent::GenerateDecalMesh(const UWorld& World)
{
	ClearDecalMeshInternal(false);

	auto FinalizeGeneratedMesh = [this]()
	{
		// RenderState 재생성 없이 Mesh dirty만 전달해 프록시가 새 버퍼 포인터를 반영하도록 한다.
		MarkProxyDirty(EDirtyFlag::Mesh);
	};

	const FBoundingBox DecalWorldAABB = GetWorldBoundingBox();
	if (!DecalWorldAABB.IsValid())
	{
		FinalizeGeneratedMesh();
		return;
	}

	if (DecalSize.X <= 0.001f || DecalSize.Y <= 0.001f || DecalSize.Z <= 0.001f)
	{
		FinalizeGeneratedMesh();
		return;
	}

	// 1. Broad Phase: WorldPrimitivePickingBVH를 이용해 데칼 AABB 주변 컴포넌트 수집
	const FWorldPrimitivePickingBVH& WorldBVH = World.EnsureAndGetWorldPrimitivePickingBVH();
	TArray<UStaticMeshComponent*> Candidates;
	WorldBVH.QueryAABB(DecalWorldAABB, Candidates);

	if (Candidates.empty())
	{
		FinalizeGeneratedMesh();
		return;
	}

	// 2. 공통 공간 변환 및 클리핑 평면 준비
	const FMatrix DecalWorldMatrix = GetTransformIncludingDecalSize();
	const FMatrix DecalInverseWorldMatrix = DecalWorldMatrix.GetInverse();

	FClipPlane Planes[6] = {
		{ FVector(1,  0,  0), -0.5f }, { FVector(-1,  0,  0), -0.5f },
		{ FVector(0,  1,  0), -0.5f }, { FVector(0, -1,  0), -0.5f },
		{ FVector(0,  0,  1), -0.5f }, { FVector(0,  0, -1), -0.5f }
	};

	TArray<FVertexPNCT> CurrentPoly;
	TArray<FVertexPNCT> ClippedPoly;

	// 3. 필터링된 각 컴포넌트에 대해 처리
	for (UStaticMeshComponent* TargetComponent : Candidates)
	{
		if (!TargetComponent) continue;
		UStaticMesh* StaticMesh = TargetComponent->GetStaticMesh();
		if (!StaticMesh) continue;

		const FStaticMesh* MeshAsset = StaticMesh->GetStaticMeshAsset();
		if (!MeshAsset || MeshAsset->Indices.empty() || MeshAsset->Vertices.empty()) continue;

		// 메쉬 BVH 빌드 보장
		StaticMesh->EnsureMeshTrianglePickingBVHBuilt();

		// 데칼 World AABB를 타겟 메쉬의 Local 공간으로 변환
		const FMatrix TargetWorldMatrix = TargetComponent->GetWorldMatrix();
		const FMatrix TargetInverseWorldMatrix = TargetComponent->GetWorldInverseMatrix();

		FBoundingBox MeshLocalBox;
		FVector WorldCorners[8];
		DecalWorldAABB.GetCorners(WorldCorners);
		for (const FVector& C : WorldCorners)
		{
			MeshLocalBox.Expand(TargetInverseWorldMatrix.TransformPositionWithW(C));
		}

		// 4. Narrow Phase: 메시의 로컬 BVH를 활용해 겹칠 가능성이 있는 삼각형 인덱스 추출
		TArray<int32> OverlappingTriIndices;
		StaticMesh->GetMeshTrianglePickingBVH().QueryAABBLocalIndices(MeshLocalBox, OverlappingTriIndices);

		if (OverlappingTriIndices.empty()) continue;

		FMatrix TargetToDecalLocal = TargetWorldMatrix * DecalInverseWorldMatrix;

		// 5. 무차별 순회가 아닌, 추출된 삼각형들에 대해서만 클리핑(SAT + 생성 역할) 진행
		for (int32 TriStartIndex : OverlappingTriIndices)
		{
			if (TriStartIndex < 0 || TriStartIndex + 2 >= (int32)MeshAsset->Indices.size()) continue;

			const FNormalVertex& V0 = MeshAsset->Vertices[MeshAsset->Indices[TriStartIndex]];
			const FNormalVertex& V1 = MeshAsset->Vertices[MeshAsset->Indices[TriStartIndex + 1]];
			const FNormalVertex& V2 = MeshAsset->Vertices[MeshAsset->Indices[TriStartIndex + 2]];

			FVector FaceNormal = FVector::Cross(V1.pos - V0.pos, V2.pos - V0.pos);
			float NormalLenSq = FaceNormal.Dot(FaceNormal);
			if (NormalLenSq < 0.000001f) continue;
			FaceNormal = FaceNormal * (1.0f / std::sqrt(NormalLenSq));

			CurrentPoly.clear();

			// 삼각형의 3 정점을 데칼 로컬 공간으로 변환
			for (int j = 0; j < 3; ++j)
			{
				const FNormalVertex& TargetVert = MeshAsset->Vertices[MeshAsset->Indices[TriStartIndex + j]];
				FVertexPNCT Vert;

				Vert.Position = TargetToDecalLocal.TransformPositionWithW(TargetVert.pos);
				Vert.Normal = TargetToDecalLocal.TransformVector(FaceNormal);

				float nLenSq = Vert.Normal.Dot(Vert.Normal);
				if (nLenSq > 0.000001f) Vert.Normal = Vert.Normal * (1.0f / std::sqrt(nLenSq));
				else Vert.Normal = FVector(0, 0, 1);

				FVector TangentDir = FVector::Cross(Vert.Normal, FVector(0, 1, 0));
				if (TangentDir.Dot(TangentDir) < 0.001f) TangentDir = FVector::Cross(Vert.Normal, FVector(0, 0, 1));

				float tLenSq = TangentDir.Dot(TangentDir);
				if (tLenSq > 0.000001f) TangentDir = TangentDir * (1.0f / std::sqrt(tLenSq));
				else TangentDir = FVector(1, 0, 0);

				Vert.Tangent = FVector4(TangentDir.X, TangentDir.Y, TangentDir.Z, 1.0f);
				Vert.Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
				CurrentPoly.push_back(Vert);
			}

			// 6평면 클리핑
			for (int p = 0; p < 6; ++p)
			{
				ClipPolygonAgainstPlane(Planes[p], CurrentPoly, ClippedPoly);
				CurrentPoly = ClippedPoly;
				if (CurrentPoly.empty()) break;
			}

			// 삼각화 및 버퍼 기록
			if (CurrentPoly.size() >= 3)
			{
				uint32 StartIndex = (uint32)GeneratedPNCTData.Vertices.size();

				for (FVertexPNCT& Vert : CurrentPoly)
				{
					// Decal.hlsl의 투영 규칙(local YZ)을 GeometryDecal의 UV에도 동일하게 적용한다.
					// X축은 프로젝션 깊이축이므로 UV에 포함하면 깊이에 따라 텍스처가 왜곡된다.
					Vert.UV.X = Vert.Position.Y + 0.5f;
					Vert.UV.Y = -Vert.Position.Z + 0.5f;
					Vert.Position += Vert.Normal * DepthBiasOffset;
					GeneratedPNCTData.Vertices.push_back(Vert);
				}

				for (size_t v = 1; v < CurrentPoly.size() - 1; ++v)
				{
					GeneratedPNCTData.Indices.push_back(StartIndex);
					GeneratedPNCTData.Indices.push_back(StartIndex + (uint32)v);
					GeneratedPNCTData.Indices.push_back(StartIndex + (uint32)v + 1);
				}
			}
		}
	}

	// 6. 모든 타겟을 순회한 후 최종 병합된 버퍼 생성
	if (!GeneratedPNCTData.Vertices.empty())
	{
		if (!GEngine)
		{
			FinalizeGeneratedMesh();
			return;
		}

		ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
		if (!Device)
		{
			FinalizeGeneratedMesh();
			return;
		}

		if (!GeneratedMeshBuffer)
		{
			GeneratedMeshBuffer = new FMeshBuffer();
		}
		GeneratedMeshBuffer->Create(Device, GeneratedPNCTData);
	}

	FinalizeGeneratedMesh();
}

void UGeometryDecalComponent::ClearDecalMesh()
{
	ClearDecalMeshInternal(true);
}

void UGeometryDecalComponent::ClearDecalMeshInternal(bool bMarkMeshDirty)
{
	GeneratedPNCTData.Vertices.clear();
	GeneratedPNCTData.Indices.clear();

	// 스마트 포인터 대신 명시적으로 동적 할당 해제
	if (GeneratedMeshBuffer)
	{
		delete GeneratedMeshBuffer;
		GeneratedMeshBuffer = nullptr;
	}

	if (bMarkMeshDirty)
	{
		MarkProxyDirty(EDirtyFlag::Mesh);
	}
}

// ==============================================================================
// Overrides
// ==============================================================================
void UGeometryDecalComponent::UpdateWorldAABB() const
{
	FVector LExt = DecalSize * 0.5f;
	FMatrix worldMatrix = GetWorldMatrix();

	float NewEx = std::abs(worldMatrix.M[0][0]) * LExt.X + std::abs(worldMatrix.M[1][0]) * LExt.Y + std::abs(worldMatrix.M[2][0]) * LExt.Z;
	float NewEy = std::abs(worldMatrix.M[0][1]) * LExt.X + std::abs(worldMatrix.M[1][1]) * LExt.Y + std::abs(worldMatrix.M[2][1]) * LExt.Z;
	float NewEz = std::abs(worldMatrix.M[0][2]) * LExt.X + std::abs(worldMatrix.M[1][2]) * LExt.Y + std::abs(worldMatrix.M[2][2]) * LExt.Z;

	FVector WorldCenter = GetWorldLocation();
	WorldAABBMinLocation = WorldCenter - FVector(NewEx, NewEy, NewEz);
	WorldAABBMaxLocation = WorldCenter + FVector(NewEx, NewEy, NewEz);

	bWorldAABBDirty = false;
	bHasValidWorldAABB = true;
}

bool UGeometryDecalComponent::LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult)
{
	if (GeneratedPNCTData.Indices.empty()) return false;

	const FMatrix DecalWorldMatrix = GetTransformIncludingDecalSize();

	const bool bHit = FRayUtils::RaycastTriangles
	(Ray, DecalWorldMatrix, DecalWorldMatrix.GetInverse(), &GeneratedPNCTData.Vertices[0].Position, sizeof(FVertexPNCT), GeneratedPNCTData.Indices, OutHitResult);

	if (bHit) OutHitResult.HitComponent = this;
	return bHit;
}

FMeshBuffer* UGeometryDecalComponent::GetMeshBuffer() const
{
	return GeneratedMeshBuffer;
}

const FMeshData* UGeometryDecalComponent::GetMeshData() const
{
	return nullptr;
}

FPrimitiveSceneProxy* UGeometryDecalComponent::CreateSceneProxy()
{
	return new FGeometryDecalSceneProxy(this);
}

void UGeometryDecalComponent::DestroyRenderState()
{
	// 생성된 데칼 메시는 RenderState 소멸과 분리해 관리한다.
	// 메모리 해제는 명시적 ClearDecalMesh 또는 소멸자에서 수행한다.
	UPrimitiveComponent::DestroyRenderState();
}

void UGeometryDecalComponent::BeginPlay()
{
	UMeshComponent::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		GenerateDecalMesh(*World);
	}
}

void UGeometryDecalComponent::OnTransformDirty()
{
	UPrimitiveComponent::OnTransformDirty();

	if (UWorld* World = GetWorld())
	{
		GenerateDecalMesh(*World);
	}
}

// ==============================================================================
// Serialization & Editor
// ==============================================================================
void UGeometryDecalComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UPrimitiveComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Decal Size", EPropertyType::Vec3, &DecalSize });
	OutProps.push_back({ "Decal Material", EPropertyType::MaterialSlot, &DecalMaterialSlot });
	OutProps.push_back({ "Depth Bias", EPropertyType::Float, &DepthBiasOffset });
}

void UGeometryDecalComponent::PostEditProperty(const char* PropertyName)
{
	UPrimitiveComponent::PostEditProperty(PropertyName);
	if (std::strcmp(PropertyName, "Decal Size") == 0)
	{
		SetDecalSize(DecalSize);
	}
	else if (std::strcmp(PropertyName, "Decal Material") == 0 || std::strcmp(PropertyName, "Element 0") == 0)
	{
		if (DecalMaterialSlot.Path.empty() || DecalMaterialSlot.Path == "None")
		{
			SetMaterial(nullptr);
		}
		else if (IsGeometryDecalTextureMaterialPath(DecalMaterialSlot.Path))
		{
			SetDecalTexture(FName(GetGeometryDecalTextureNameFromMaterialPath(DecalMaterialSlot.Path)));
		}
		else
		{
			SetMaterial(FObjManager::GetOrLoadMaterial(DecalMaterialSlot.Path));
		}
	}
	else if (std::strcmp(PropertyName, "Depth Bias") == 0)
	{
		if (UWorld* World = GetWorld())
		{
			GenerateDecalMesh(*World);
		}
		else
		{
			MarkProxyDirty(EDirtyFlag::Mesh);
		}
	}
}

void UGeometryDecalComponent::Serialize(FArchive& Ar)
{
	UMeshComponent::Serialize(Ar);
	Ar << DecalSize << DecalMaterialSlot.Path << DecalMaterialSlot.bUVScroll << DepthBiasOffset;
}

void UGeometryDecalComponent::PostDuplicate()
{
	UMeshComponent::PostDuplicate();

	if (IsGeometryDecalTextureMaterialPath(DecalMaterialSlot.Path))
	{
		SetDecalTexture(FName(GetGeometryDecalTextureNameFromMaterialPath(DecalMaterialSlot.Path)));
	}
	else if (!DecalMaterialSlot.Path.empty() && DecalMaterialSlot.Path != "None")
	{
		SetMaterial(FObjManager::GetOrLoadMaterial(DecalMaterialSlot.Path));
	}
	else
	{
		SetMaterial(nullptr);
	}

	if (UWorld* World = GetWorld())
	{
		GenerateDecalMesh(*World);
	}
	else
	{
		MarkProxyDirty(EDirtyFlag::Mesh);
	}
}


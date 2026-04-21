#pragma once

#include "Components/MeshComponent.h"
#include "Core/PropertyTypes.h"
#include "Core/ResourceTypes.h"
#include "Render/Types/VertexTypes.h"
#include "Render/Resource/MeshBufferManager.h"
#include <memory>

class UMaterialInterface;
class FPrimitiveSceneProxy;
class UWorld;

class UGeometryDecalComponent : public UMeshComponent
{
public:
	DECLARE_CLASS(UGeometryDecalComponent, UMeshComponent)

	UGeometryDecalComponent();
	~UGeometryDecalComponent() override;

	// === Geometry Decal 핵심 함수 ===
	// 씬(World) 전체의 BVH를 검색하여 데칼 범위 내의 폴리곤을 수집하고 클리핑된 메쉬를 생성합니다.
	void GenerateDecalMesh(const UWorld& World);
	void ClearDecalMesh();

	// === 속성 설정 ===
	void SetDecalSize(const FVector& InSize);
	FMatrix GetTransformIncludingDecalSize() const;

	void SetMaterial(UMaterialInterface* InMaterial);
	UMaterialInterface* GetMaterial() const { return DecalMaterial; }
	void SetDecalTexture(const FName& TextureName);
	const FTextureResource* GetDecalTexture() const { return DecalTexture; }
	bool IsUVScrollEnabled() const { return DecalMaterialSlot.bUVScroll; }

	// === UPrimitiveComponent 오버라이드 ===
	void UpdateWorldAABB() const override;
	bool LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult) override;

	FMeshBuffer* GetMeshBuffer() const override;
	const FMeshData* GetMeshData() const override;
	FPrimitiveSceneProxy* CreateSceneProxy() override;
	void DestroyRenderState() override;
	void BeginPlay() override;

	// === 에디터 및 직렬화 ===
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;
	void Serialize(FArchive& Ar) override;
	void PostDuplicate() override;

protected:
	struct FClipPlane { FVector Normal; float D; };

	void OnTransformDirty() override;
	void ClipPolygonAgainstPlane(const FClipPlane& Plane, const TArray<FVertexPNCT>& InPoly, TArray<FVertexPNCT>& OutPoly);
	FVertexPNCT InterpolateVertex(const FVertexPNCT& V1, const FVertexPNCT& V2, float t);
	void ClearDecalMeshInternal(bool bMarkMeshDirty);

private:
	FVector DecalSize = FVector(1.0f, 1.0f, 1.0f);
	UMaterialInterface* DecalMaterial = nullptr;
	FMaterialSlot DecalMaterialSlot;
	FName DecalTextureName;
	FTextureResource* DecalTexture = nullptr;
	float DepthBiasOffset = 0.005f;

	TMeshData<FVertexPNCT> GeneratedPNCTData;
	FMeshBuffer* GeneratedMeshBuffer = nullptr;
};

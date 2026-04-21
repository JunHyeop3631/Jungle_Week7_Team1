#include "GameFramework/GeometryDecalActor.h"
#include "Object/ObjectFactory.h"
#include "Components/GeometryDecalComponent.h"
#include "Components/BillboardComponent.h"
#include "Mesh/ObjManager.h"
#include "GameFramework/World.h"

#include <cstring>

IMPLEMENT_CLASS(AGeometryDecalActor, AActor)

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
}

AGeometryDecalActor::AGeometryDecalActor()
{
	GeometryDecal = AddComponent<UGeometryDecalComponent>();
	SetRootComponent(GeometryDecal);

	SpriteComponent = AddComponent<UBillboardComponent>();
	SpriteComponent->AttachToComponent(GeometryDecal);
	// 필요한 경우 아이콘 변경 (예: GeometryDecalIcon)
	SpriteComponent->SetTexture(FName("DecalIcon"));
}

void AGeometryDecalActor::SetDecalMaterial(UMaterialInterface* NewDecalMaterial)
{
	if (GeometryDecal)
	{
		GeometryDecal->SetMaterial(NewDecalMaterial);
	}
}

void AGeometryDecalActor::SetDecalMaterial(const FString& MaterialPath)
{
	if (MaterialPath.empty() || MaterialPath == "None")
	{
		SetDecalMaterial(static_cast<UMaterialInterface*>(nullptr));
		return;
	}

	if (GeometryDecal && IsGeometryDecalTextureMaterialPath(MaterialPath))
	{
		GeometryDecal->SetDecalTexture(FName(GetGeometryDecalTextureNameFromMaterialPath(MaterialPath)));
		return;
	}

	SetDecalMaterial(FObjManager::GetOrLoadMaterial(MaterialPath));
}

UMaterialInterface* AGeometryDecalActor::GetDecalMaterial() const
{
	return GeometryDecal ? GeometryDecal->GetMaterial() : nullptr;
}

void AGeometryDecalActor::SetDecalSize(const FVector& InDecalSize)
{
	if (GeometryDecal)
	{
		GeometryDecal->SetDecalSize(InDecalSize);
	}
}

void AGeometryDecalActor::GenerateDecalMesh()
{
	if (GeometryDecal && GetWorld())
	{
		GeometryDecal->GenerateDecalMesh(*GetWorld());
	}
}

void AGeometryDecalActor::BeginPlay()
{
	AActor::BeginPlay();

	for (UActorComponent* Component : GetComponents())
	{
		if (UBillboardComponent* Billboard = Cast<UBillboardComponent>(Component))
		{
			Billboard->SetVisibility(false);
		}
	}
}

void AGeometryDecalActor::Serialize(FArchive& Ar)
{
	AActor::Serialize(Ar);
}

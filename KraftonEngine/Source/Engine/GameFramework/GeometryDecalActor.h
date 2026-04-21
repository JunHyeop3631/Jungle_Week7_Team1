#pragma once

#include "GameFramework/AActor.h"

class UBillboardComponent;
class UGeometryDecalComponent;
class UMaterialInterface;

class AGeometryDecalActor : public AActor
{
public:
	DECLARE_CLASS(AGeometryDecalActor, AActor)

	AGeometryDecalActor();

	UGeometryDecalComponent* GetGeometryDecal() const { return GeometryDecal; }
	UBillboardComponent* GetSpriteComponent() const { return SpriteComponent; }

	// Component의 속성 설정 래핑 함수
	void SetDecalMaterial(UMaterialInterface* NewDecalMaterial);
	void SetDecalMaterial(const FString& MaterialPath);
	UMaterialInterface* GetDecalMaterial() const;

	void SetDecalSize(const FVector& InDecalSize);

	// 에디터 런타임에서 지오메트리를 잘라내어 메쉬를 생성하도록 지시합니다.
	void GenerateDecalMesh();

	void BeginPlay() override;
	virtual void Serialize(FArchive& Ar) override;

protected:
	UGeometryDecalComponent* GeometryDecal = nullptr;
	UBillboardComponent* SpriteComponent = nullptr; // Editor 가이드용
};
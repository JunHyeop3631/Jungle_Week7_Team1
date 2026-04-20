#pragma once
#include "AActor.h"

class UPointLightComponent;
class UBillboardComponent;

class APointLight : public AActor
{
public:
	DECLARE_CLASS(APointLight, AActor)
	
	APointLight();

	void BeginPlay() override;
	void Serialize(FArchive& Ar) override;

private:
	UBillboardComponent* SpriteComponent = nullptr;
	UPointLightComponent* PointLight = nullptr;
};


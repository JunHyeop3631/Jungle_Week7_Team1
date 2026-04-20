#pragma once
#include "AActor.h"

class UBillboardComponent;
class USpotLightComponent;

class ASpotLight : public AActor
{
public:
	DECLARE_CLASS(ASpotLight, AActor)

	ASpotLight();


	void BeginPlay() override;
	void Serialize(FArchive& Ar) override;

private:
	UBillboardComponent* SpriteComponent = nullptr;
	USpotLightComponent* SpotLight = nullptr;
};


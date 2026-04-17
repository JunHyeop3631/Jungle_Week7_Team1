#pragma once
#include "AActor.h"

class UPointLightComponent;
class UBillboardComponent;

class APointLight : public AActor
{
public:
	DECLARE_CLASS(APointLight, AActor)
	
	APointLight();

private:
	UBillboardComponent* SpriteComponent = nullptr;
	UPointLightComponent* PointLight = nullptr;
};


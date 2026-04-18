#pragma once
#include "AActor.h"

class UDirectionalLightComponent;
class UBillboardComponent;

class ADirectionalLight : public AActor
{
public:
	DECLARE_CLASS(ADirectionalLight, AActor)
	ADirectionalLight();

private:
	UDirectionalLightComponent* DirectionalLight;
	UBillboardComponent* SpriteComponent;
};


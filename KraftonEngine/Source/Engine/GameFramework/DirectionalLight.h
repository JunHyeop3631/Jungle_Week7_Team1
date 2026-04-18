#pragma once
#include "AActor.h"

class UDirectionalLightComponent;
class UBillboardComponent;

class ADirectionalLight : public AActor
{
public:
	ADirectionalLight();

private:
	UDirectionalLightComponent* DirectionalLight;
	UBillboardComponent* SpriteComponent;
};


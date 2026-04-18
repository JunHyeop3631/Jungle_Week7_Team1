#pragma once
#include "AActor.h"

class UAmbientLightComponent;
class UBillboardComponent;

class AAmbientLight : public AActor
{
public:
	AAmbientLight();

private:
	UAmbientLightComponent* AmbientLight;
	UBillboardComponent* SpriteComponent;
};


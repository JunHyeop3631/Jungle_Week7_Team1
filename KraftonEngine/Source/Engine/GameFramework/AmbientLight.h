#pragma once
#include "AActor.h"

class UAmbientLightComponent;
class UBillboardComponent;

class AAmbientLight : public AActor
{
public:
	DECLARE_CLASS(AAmbientLight, AActor)

	AAmbientLight();

private:
	UAmbientLightComponent* AmbientLight;
	UBillboardComponent* SpriteComponent;
};


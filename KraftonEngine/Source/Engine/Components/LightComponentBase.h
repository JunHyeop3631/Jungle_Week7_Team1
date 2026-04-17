#pragma once
#include "SceneComponent.h"
#include "Core/EngineTypes.h"

class ULightComponentBase : public USceneComponent
{
public:

private:
	float Intensity;
	FColor LightColor;
	bool bVisible;
};

	
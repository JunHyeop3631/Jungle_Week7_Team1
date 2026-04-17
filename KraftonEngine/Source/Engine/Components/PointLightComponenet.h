#pragma once
#include "LightComponent.h"
class UPointLightComponenet : public ULightComponent
{
private:
	float AttenuationRadius;
	float LightFalloffExponent;
};


#pragma once
#include "LightComponentBase.h"

class UPointLightComponent : public ULightComponent
{

private:
    float AttenuationRadius = 0.0f;
    float LightFalloffExponent = 0.0f;
};

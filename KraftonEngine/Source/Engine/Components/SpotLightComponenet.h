#pragma once
#include "PointLightComponenet.h"
class USpotLightComponenet : public UPointLightComponenet
{
private:
	float InnerConeAngle;
	float OuterConeAngle;
};


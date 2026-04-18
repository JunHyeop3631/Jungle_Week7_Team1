#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"

// UExponentialHeightFogComponent와 FogPostProcess.hlsl이 공유하는 포맷입니다.
struct FFogUniformParameters
{
	FVector4 ExponentialFogParameters;      // x: Density, y: HeightFalloff, z: reserved, w: StartDistance
	FVector4 ExponentialFogColorParameter;  // rgb: Inscattering Color, a: 1.0 - MaxOpacity
	FVector4 ExponentialFogParameters3;     // x: reserved, y: FogHeight, z: reserved, w: CutoffDistance
};

struct FFogPostProcessConstants
{
	FFogUniformParameters Fogs[8];
	uint32 FogCount = 0;
	float _pad[3] = {};
};

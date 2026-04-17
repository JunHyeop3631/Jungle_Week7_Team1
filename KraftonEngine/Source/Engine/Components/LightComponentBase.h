#pragma once
#include "SceneComponent.h"
#include "Core/EngineTypes.h"

struct FLightData;

class ULightComponentBase : public USceneComponent
{
public:
    DECLARE_CLASS(ULightComponentBase, USceneComponent)

    void CreateRenderState() override;

	float GetIntensiry() const { return Intensity; }
	FVector GetLightColor() const { return LightColor; }
	float GetRadius() const { return Radius; }

    void SetIntensity(float InIntensity) { Intensity = InIntensity; }
    void SetLightColor(FVector InColor) { LightColor = InColor; }
    void SetRadius(float InRadius) { Radius = InRadius; }

	bool IsVisible() const { return bVisible; }


private:
    float Intensity = 0.f;
    FVector LightColor = {0, 0, 0};
    float Radius = 0.0f;
    bool bVisible = false;

    FLightData* lightData = nullptr;
};

class ULightComponent : public ULightComponentBase
{
public: 
    DECLARE_CLASS(ULightComponent, ULightComponentBase)
};
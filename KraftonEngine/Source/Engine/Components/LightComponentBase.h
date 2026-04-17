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
	FColor GetLightColor() const { return LightColor; }
	bool IsVisible() const { return bVisible; }


private:
    float Intensity = 0.f;
    FColor LightColor = {0, 0, 0, 0};
    bool bVisible = false;

    FLightData* lightData = nullptr;
};

class ULightComponent : public ULightComponentBase
{
public: 
    DECLARE_CLASS(ULightComponent, ULightComponentBase)
};
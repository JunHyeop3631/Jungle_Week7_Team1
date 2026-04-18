#pragma once
#include "LightComponent.h"

class ULocalLightComponent : public ULightComponent
{
public:
	DECLARE_CLASS(ULocalLightComponent, ULightComponent)

	// Getter Setter
	float	GetAttenuationRadius() const { return AttenuationRadius; }
	void	SetAttenuationRadius(float NewRadius) { AttenuationRadius = NewRadius; MarkProxyDirty(EDirtyFlag::LightData); }

	FLightSceneProxy* CreateLightSceneProxy() override;

	// Override
	void	GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void	PostEditProperty(const char* PropertyName) override;
	void	Serialize(FArchive& Ar) override;

private:
	// 감쇠 반경
	float AttenuationRadius = 1.f;
};


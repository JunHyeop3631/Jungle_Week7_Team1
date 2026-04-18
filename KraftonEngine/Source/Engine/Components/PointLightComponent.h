#pragma once
#include "LocalLightComponent.h"
class UPointLightComponent : public ULocalLightComponent
{
public:
	DECLARE_CLASS(UPointLightComponent, ULocalLightComponent)


	// Getter Setter
	float GetLightFalloffExponent() const { return LightFalloffExponent; }
	void SetLightFalloffExponent(float NewFalloff) { LightFalloffExponent = NewFalloff; MarkProxyDirty(EDirtyFlag::LightData); }

	FLightSceneProxy* CreateLightSceneProxy() override;

	// Override
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;
	void Serialize(FArchive& Ar) override;
	void CollectEditorVisualizations(FRenderBus& RenderBus) const override;


	void DestroyRenderState() override;

private:
	float LightFalloffExponent = 1.f;
};


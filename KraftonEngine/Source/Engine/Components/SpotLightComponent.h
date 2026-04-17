#pragma once
#include "PointLightComponent.h"
class USpotLightComponent : public UPointLightComponent
{
public:
	DECLARE_CLASS(USpotLightComponent, UPointLightComponent)

	// Getter Setter
	float GetInnerConeAngle() const {return InnerConeAngle;}
	void SetInnerConeAngle(float NewInnerConeAngle) { InnerConeAngle = NewInnerConeAngle; }
	float GetOuterConeAngle() const { return OuterConeAngle; }
	void SetOuterConeAngle(float NewOuterConeangle) { OuterConeAngle = NewOuterConeangle; }

	void CollectEditorVisualizations(FRenderBus& RenderBus) const override;


	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;
	void Serialize(FArchive& Ar) override;

private:
	float InnerConeAngle = 0.f;
	float OuterConeAngle = 30.f;
};


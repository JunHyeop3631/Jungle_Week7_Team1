#pragma once
#include "LightComponent.h"
class UDirectionalLightComponent : public ULightComponent
{
public:
	DECLARE_CLASS(UDirectionalLightComponent, ULightComponent)

	FLightSceneProxy* CreateLightSceneProxy() override;

	void CollectEditorVisualizations(FRenderBus& RenderBus) const override;

};


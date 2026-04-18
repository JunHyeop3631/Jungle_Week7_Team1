#include "DirectionalLightComponent.h"
#include "Object/ObjectFactory.h"
#include "Render/Proxy/LightSceneProxy.h"

IMPLEMENT_CLASS(UDirectionalLightComponent, ULightComponent)

FLightSceneProxy* UDirectionalLightComponent::CreateLightSceneProxy()
{
	return new FDirectionalLightSceneProxy(this);
}

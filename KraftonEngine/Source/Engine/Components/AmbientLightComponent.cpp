#include "AmbientLightComponent.h"
#include "Object/ObjectFactory.h"
#include "Render/Proxy/LightSceneProxy.h"

IMPLEMENT_CLASS(UAmbientLightComponent, ULightComponent)

FLightSceneProxy* UAmbientLightComponent::CreateLightSceneProxy()
{
	return new FAmbientLightSceneProxy(this);
}

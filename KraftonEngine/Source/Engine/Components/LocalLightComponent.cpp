#include "LocalLightComponent.h"
#include "Object/ObjectFactory.h"
#include "Render/Proxy/LightSceneProxy.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(ULocalLightComponent, ULightComponent)

FLightSceneProxy* ULocalLightComponent::CreateLightSceneProxy()
{
	return new FLocalLightSceneProxy(this);
}

void ULocalLightComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	ULightComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Attenuation Radius", EPropertyType::Float, &AttenuationRadius, 0.f, 20.f});
}

void ULocalLightComponent::PostEditProperty(const char* PropertyName)
{
	ULightComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "Attenuation Radius") == 0)
	{
		MarkProxyDirty(EDirtyFlag::LightData);
	}
}

void ULocalLightComponent::Serialize(FArchive& Ar)
{
	ULightComponent::Serialize(Ar);
	Ar << AttenuationRadius;
}

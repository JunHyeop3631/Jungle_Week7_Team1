#include "LocalLightComponent.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(ULocalLightComponent, ULightComponent)

void ULocalLightComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	ULightComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Attenuation Radius", EPropertyType::Float, &AttenuationRadius });
}

void ULocalLightComponent::PostEditProperty(const char* PropertyName)
{
	ULightComponent::PostEditProperty(PropertyName);
}

void ULocalLightComponent::Serialize(FArchive& Ar)
{
	ULightComponent::Serialize(Ar);
	Ar << AttenuationRadius;
}

void ULocalLightComponent::SetTintColor()
{

}

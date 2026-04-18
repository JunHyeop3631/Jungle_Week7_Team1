#include "LightSceneProxy.h"
#include "Components/AmbientLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/LightComponentBase.h"
#include "Components/LocalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "GameFramework/AActor.h"

FLightSceneProxy::FLightSceneProxy(ULightComponentBase* InComponent)
	: Owner(InComponent)
{
}

void FLightSceneProxy::UpdateTransform()
{
	if (!Owner)
	{
		return;
	}

	CachedTransform = FTransform(Owner->GetWorldLocation(), Owner->GetWorldQuat(), Owner->GetWorldScale());
}

void FLightSceneProxy::UpdateVisibility()
{
	if (!Owner)
	{
		bVisible = false;
		return;
	}

	bVisible = Owner->IsVisible();
	if (bVisible)
	{
		AActor* OwnerActor = Owner->GetOwner();
		if (OwnerActor && !OwnerActor->IsVisible())
		{
			bVisible = false;
		}
	}
}

void FLightSceneProxy::UpdateLightData()
{
	if (!Owner)
	{
		return;
	}

	CachedColor = Owner->GetLightColor();
	CachedIntensity = Owner->GetIntensity();
}

FAmbientLightSceneProxy::FAmbientLightSceneProxy(UAmbientLightComponent* InComponent)
	: FLightSceneProxy(InComponent)
{
}

FDirectionalLightSceneProxy::FDirectionalLightSceneProxy(UDirectionalLightComponent* InComponent)
	: FLightSceneProxy(InComponent)
{
}

FLocalLightSceneProxy::FLocalLightSceneProxy(ULocalLightComponent* InComponent)
	: FLightSceneProxy(InComponent)
{
}

void FLocalLightSceneProxy::UpdateLightData()
{
	FLightSceneProxy::UpdateLightData();

	const ULocalLightComponent* LocalLight = static_cast<const ULocalLightComponent*>(Owner);
	if (!LocalLight)
	{
		return;
	}

	CachedAttenuationRadius = LocalLight->GetAttenuationRadius();
}

FPointLightSceneProxy::FPointLightSceneProxy(UPointLightComponent* InComponent)
	: FLocalLightSceneProxy(InComponent)
{
}

void FPointLightSceneProxy::UpdateLightData()
{
	FLocalLightSceneProxy::UpdateLightData();

	const UPointLightComponent* PointLight = static_cast<const UPointLightComponent*>(Owner);
	if (!PointLight)
	{
		return;
	}

	CachedFalloffExponent = PointLight->GetLightFalloffExponent();
}

FSpotLightSceneProxy::FSpotLightSceneProxy(USpotLightComponent* InComponent)
	: FPointLightSceneProxy(InComponent)
{
}

void FSpotLightSceneProxy::UpdateLightData()
{
	FPointLightSceneProxy::UpdateLightData();

	const USpotLightComponent* SpotLight = static_cast<const USpotLightComponent*>(Owner);
	if (!SpotLight)
	{
		return;
	}

	CachedInnerConeAngle = SpotLight->GetInnerConeAngle();
	CachedOuterConeAngle = SpotLight->GetOuterConeAngle();
}

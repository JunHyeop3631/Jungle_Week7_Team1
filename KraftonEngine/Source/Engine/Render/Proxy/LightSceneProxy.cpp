#include "LightSceneProxy.h"
#include "Components/LightComponentBase.h"
#include "GameFramework/AActor.h"

FLightSceneProxy::FLightSceneProxy(ULightComponentBase* InComponent)
	:Owner(InComponent)
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

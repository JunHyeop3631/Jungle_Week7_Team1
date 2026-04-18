#include "LightSceneProxy.h"
#include "Components/AmbientLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/LightComponentBase.h"
#include "Components/LocalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "GameFramework/AActor.h"

namespace
{
	// ===============================================
	// Light Scene Proxy Helper
	// ===============================================
	FVector4 BuildLightColor(const FLightSceneProxy* Proxy)
	{
		if (!Proxy)
		{
			return FVector4(0.f, 0.f, 0.f, 1.f);
		}
		const FVector4& Color = Proxy->CachedColor.ToVector4();
		const float Intensity = Proxy->CachedIntensity;
		return Color * Intensity;
	}

	FVector4 BuildLightPosition(const FLightSceneProxy* Proxy)
	{
		if (!Proxy)
		{
			return FVector4(0.f, 0.f, 0.f, 1.f);
		}

		const FVector& Position = Proxy->CachedTransform.Location;
		return FVector4(Position.X, Position.Y, Position.Z, 1.0f);
	}

	FVector4 BuildLightDirection(const FLightSceneProxy* Proxy)
	{
		if (!Proxy)
		{
			return FVector4(0.0f, -1.0f, 0.0f, 0.0f);
		}

		const FVector Direction = Proxy->CachedTransform.Rotation.GetForwardVector().Normalized();
		return FVector4(Direction.X, Direction.Y, Direction.Z, 0.0f);
	}

	float DegreesToConeCos(float Degrees)
	{
		return cosf(Degrees * FMath::DegToRad);
	}
}

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

void FAmbientLightSceneProxy::CollectEntries(FLightingBuildContext& Context, FLightingConstants& OutResult)
{
	if (Context.bHasAmbient)
	{
		return;
	}
	OutResult.Ambient.LightColor = BuildLightColor(this);
	Context.bHasAmbient = true;
}

FDirectionalLightSceneProxy::FDirectionalLightSceneProxy(UDirectionalLightComponent* InComponent)
	: FLightSceneProxy(InComponent)
{
}

void FDirectionalLightSceneProxy::CollectEntries(FLightingBuildContext& Context, FLightingConstants& OutResult)
{
	if (Context.bHasDirectional)
	{
		return;
	}
	OutResult.Directional.LightColor = BuildLightColor(this);
	OutResult.Directional.Direction = BuildLightDirection(this);
	Context.bHasDirectional = true;
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

void FPointLightSceneProxy::CollectEntries(FLightingBuildContext& Context, FLightingConstants& OutResult)
{
	if (Context.PointCount >= NUM_POINT_LIGHT)
	{
		return;
	}

	FPointLightInfo& PointInfo = OutResult.PointLights[Context.PointCount++];
	PointInfo.LightColor = BuildLightColor(this);
	PointInfo.Position = BuildLightPosition(this);
	PointInfo.AttenuationRadius = CachedAttenuationRadius;
	PointInfo.FalloffExponent = CachedFalloffExponent;
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

void FSpotLightSceneProxy::CollectEntries(FLightingBuildContext& Context, FLightingConstants& OutResult)
{
	if (Context.SpotCount >= NUM_SPOT_LIGHT)
	{
		return;;
	}

	FSpotLightInfo& SpotInfo = OutResult.SpotLights[Context.SpotCount++];
	SpotInfo.LightColor = BuildLightColor(this);
	SpotInfo.Position = BuildLightPosition(this);
	SpotInfo.Direction = BuildLightDirection(this);
	SpotInfo.AttenuationRadius = CachedAttenuationRadius;
	SpotInfo.InnerConeAngle = DegreesToConeCos(CachedInnerConeAngle);
	SpotInfo.OuterConeAngle = DegreesToConeCos(CachedOuterConeAngle);
}

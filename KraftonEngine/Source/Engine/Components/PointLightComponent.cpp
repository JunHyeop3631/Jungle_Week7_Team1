#include "PointLightComponent.h"
#include "Serialization/Archive.h"
#include "Object/ObjectFactory.h"
#include "Render/Pipeline/RenderBus.h"
#include "Render/Pipeline/RenderConstants.h"
#include "Render/Proxy/LightSceneProxy.h"

#include <cmath>

namespace
{
#pragma region 디버그 라인
	constexpr float Pi = 3.14159265f;
	constexpr float TwoPi = Pi * 2.0f;
	constexpr int32 SphereSegmentCount = 24;
	constexpr float MinSphereRadius = 1e-6f;

	void AddDebugLine(FRenderBus& RenderBus, const FVector& Start, const FVector& End, const FLinearColor& Color)
	{
		FDebugLineEntry Line;
		Line.Start = Start;
		Line.End = End;
		Line.Color = Color.ToVector4();
		RenderBus.AddDebugLineEntry(std::move(Line));
	}

	void AddSphere(FRenderBus& RenderBus, const FVector& Center, float Radius)
	{
		if (Radius <= MinSphereRadius)
		{
			return;
		}

		const FLinearColor SphereColor = FLinearColor::White();

		auto AddCircle = [&](const FVector& AxisA, const FVector& AxisB)
		{
			FVector PreviousPoint = Center + AxisA * Radius;

			for (int32 SegmentIndex = 1; SegmentIndex <= SphereSegmentCount; ++SegmentIndex)
			{
				const float Angle = (TwoPi * static_cast<float>(SegmentIndex)) / static_cast<float>(SphereSegmentCount);
				const float CosAngle = cosf(Angle);
				const float SinAngle = sinf(Angle);
				const FVector CurrentPoint = Center + (AxisA * CosAngle + AxisB * SinAngle) * Radius;

				AddDebugLine(RenderBus, PreviousPoint, CurrentPoint, SphereColor);
				PreviousPoint = CurrentPoint;
			}
		};

		AddCircle(FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f));
		AddCircle(FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f));
		AddCircle(FVector(0.0f, 1.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f));
	}
#pragma endregion
}
IMPLEMENT_CLASS(UPointLightComponent, ULocalLightComponent)

FLightSceneProxy* UPointLightComponent::CreateLightSceneProxy()
{
	return new FPointLightSceneProxy(this);
}

void UPointLightComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	ULocalLightComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "LightFalloffExponent", EPropertyType::Float, &LightFalloffExponent });
}

void UPointLightComponent::PostEditProperty(const char* PropertyName)
{
	ULocalLightComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "LightFalloffExponent") == 0)
	{
		MarkProxyDirty(EDirtyFlag::LightData);
	}
}

void UPointLightComponent::Serialize(FArchive& Ar)
{
	ULocalLightComponent::Serialize(Ar);

	Ar << LightFalloffExponent;
}

void UPointLightComponent::CollectEditorVisualizations(FRenderBus& RenderBus) const
{
	const FVector Location = GetWorldLocation();
	const float Radius = GetAttenuationRadius();
	if (Radius <= MinSphereRadius)
	{
		return;
	}

	AddSphere(RenderBus, Location, Radius);
}

void UPointLightComponent::DestroyRenderState()
{
	ULocalLightComponent::DestroyRenderState();
}

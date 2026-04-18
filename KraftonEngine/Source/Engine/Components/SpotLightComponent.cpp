#include "SpotLightComponent.h"
#include "Render/Pipeline/RenderConstants.h"
#include "Render/Pipeline/RenderBus.h"
#include "Render/Proxy/LightSceneProxy.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

#include <cmath>

namespace
{	
#pragma region 디버그라인
	constexpr float Pi = 3.14159265f;
	constexpr float TwoPi = Pi * 2.0f;
	constexpr float DegreesToRadians = Pi / 180.0f;
	constexpr float MinConeLength = 1e-6f;
	constexpr int32 ConeSegmentCount = 24;
	constexpr int32 ConeSideCount = 24;

	void AddDebugLine(FRenderBus& RenderBus, const FVector& Start, const FVector& End, const FLinearColor& Color)
	{
		FDebugLineEntry Line;
		Line.Start = Start;
		Line.End = End;
		Line.Color = Color.ToVector4();
		RenderBus.AddDebugLineEntry(std::move(Line));
	}

	FVector GetStableUpVector(const FVector& Direction, const FVector& PreferredUp)
	{
		FVector StableUp = PreferredUp.Normalized();
		if (StableUp.Length() <= MinConeLength || std::abs(Direction.Dot(StableUp)) > 0.98f)
		{
			StableUp = (std::abs(Direction.Z) < 0.98f)
				? FVector(0.0f, 0.0f, 1.0f)
				: FVector(0.0f, 1.0f, 0.0f);
		}

		return StableUp;
	}

	void AddLightDirectionArrow(FRenderBus& RenderBus, const FVector& Start, const FVector& Direction, float Length, const FLinearColor InColor)
	{
		if (Length <= MinConeLength)
		{
			return;
		}

		const float DirectionLength = Direction.Length();
		if (DirectionLength <= MinConeLength)
		{
			return;
		}

		const FVector NormalizedDirection = Direction / DirectionLength;
		const FVector End = Start + NormalizedDirection * Length;
		const float HeadLength = Clamp(Length * 0.25f, 0.25f, 2.0f);
		const float HeadHalfWidth = HeadLength * 0.45f;
		const FVector StableUp = GetStableUpVector(NormalizedDirection, FVector(0.0f, 0.0f, 1.0f));
		const FVector Right = StableUp.Cross(NormalizedDirection).Normalized();
		const FVector HeadBase = End - NormalizedDirection * HeadLength;

		AddDebugLine(RenderBus, Start, End, InColor);
		AddDebugLine(RenderBus, End, HeadBase + Right * HeadHalfWidth, InColor);
		AddDebugLine(RenderBus, End, HeadBase - Right * HeadHalfWidth, InColor);
	}

	void AddCone(FRenderBus& RenderBus, const FVector& Start, const FVector& Direction, const FVector& Up, float Length, float InConeAngle)
	{
		if (Length <= MinConeLength || InConeAngle <= 0.0f)
		{
			return;
		}

		const float DirectionLength = Direction.Length();
		if (DirectionLength <= MinConeLength)
		{
			return;
		}

		const FVector NormalizedDirection = Direction / DirectionLength;
		const float ConeAngleRadians = InConeAngle * DegreesToRadians;
		if (ConeAngleRadians >= (Pi * 0.5f) - 1e-3f)
		{
			return;
		}

		FVector ConeUp = GetStableUpVector(NormalizedDirection, Up);
		FVector ConeRight = NormalizedDirection.Cross(ConeUp).Normalized();
		ConeUp = ConeRight.Cross(NormalizedDirection).Normalized();

		const FVector BaseCenter = Start + NormalizedDirection * Length;
		const float ConeRadius = tanf(ConeAngleRadians) * Length;
		if (ConeRadius <= MinConeLength)
		{
			return;
		}

		FVector CirclePoints[ConeSegmentCount];

		for (int32 SegmentIndex = 0; SegmentIndex < ConeSegmentCount; ++SegmentIndex)
		{
			const float Angle = (TwoPi * static_cast<float>(SegmentIndex)) / static_cast<float>(ConeSegmentCount);
			const float CosAngle = cosf(Angle);
			const float SinAngle = sinf(Angle);
			CirclePoints[SegmentIndex] = BaseCenter
				+ ConeRight * (ConeRadius * CosAngle)
				+ ConeUp * (ConeRadius * SinAngle);
		}

		const FLinearColor ConeColor = FLinearColor::White();
		for (int32 SegmentIndex = 0; SegmentIndex < ConeSegmentCount; ++SegmentIndex)
		{
			const FVector& CircleStart = CirclePoints[SegmentIndex];
			const FVector& CircleEnd = CirclePoints[(SegmentIndex + 1) % ConeSegmentCount];
			AddDebugLine(RenderBus, CircleStart, CircleEnd, ConeColor);
		}

		for (int32 SideIndex = 0; SideIndex < ConeSideCount; ++SideIndex)
		{
			const int32 PointIndex = (SideIndex * ConeSegmentCount) / ConeSideCount;
			AddDebugLine(RenderBus, Start, CirclePoints[PointIndex], ConeColor);
		}
	}
#pragma endregion
}
IMPLEMENT_CLASS(USpotLightComponent, UPointLightComponent)

FLightSceneProxy* USpotLightComponent::CreateLightSceneProxy()
{
	return new FSpotLightSceneProxy(this);
}

void USpotLightComponent::CollectEditorVisualizations(FRenderBus& RenderBus) const
{
	const FVector Location = GetWorldLocation();
	const FVector Forward = GetForwardVector();
	const FVector Up = GetUpVector();
	const float ConeLength = GetAttenuationRadius();
	if (ConeLength <= MinConeLength)
	{
		return;
	}

	const float ArrowLength = Clamp(ConeLength * 0.25f, 1.0f, ConeLength);

	AddLightDirectionArrow(RenderBus, Location, Forward, ArrowLength, LightColor);
	AddCone(RenderBus, Location, Forward, Up, ConeLength, InnerConeAngle);
	AddCone(RenderBus, Location, Forward, Up, ConeLength, OuterConeAngle);
}

void USpotLightComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UPointLightComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "InnerConeAngle", EPropertyType::Float, &InnerConeAngle });
	OutProps.push_back({ "OuterConeAngle", EPropertyType::Float, &OuterConeAngle });
}

void USpotLightComponent::PostEditProperty(const char* PropertyName)
{
	UPointLightComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "InnerConeAngle") == 0 || strcmp(PropertyName, "OuterConeAngle") == 0)
	{
		MarkProxyDirty(EDirtyFlag::LightData);
	}
}

void USpotLightComponent::Serialize(FArchive& Ar)
{
	UPointLightComponent::Serialize(Ar);

	Ar << InnerConeAngle;
	Ar << OuterConeAngle;
}

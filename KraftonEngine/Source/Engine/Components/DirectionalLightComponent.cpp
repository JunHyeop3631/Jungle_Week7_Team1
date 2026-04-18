#include "DirectionalLightComponent.h"
#include "Object/ObjectFactory.h"
#include "Render/Proxy/LightSceneProxy.h"
#include "Render/Pipeline/RenderBus.h"

namespace
{
#pragma region 디버그라인
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
#pragma endregion
}

IMPLEMENT_CLASS(UDirectionalLightComponent, ULightComponent)

FLightSceneProxy* UDirectionalLightComponent::CreateLightSceneProxy()
{
	return new FDirectionalLightSceneProxy(this);
}

void UDirectionalLightComponent::CollectEditorVisualizations(FRenderBus& RenderBus) const
{
	const FVector Location = GetWorldLocation();
	const FVector Forward = GetForwardVector();
	const FVector Up = GetUpVector();

	const float ArrowLength = 1.0f;

	AddLightDirectionArrow(RenderBus, Location, Forward, ArrowLength, LightColor);
}

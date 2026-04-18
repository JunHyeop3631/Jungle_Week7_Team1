#pragma once
#include "SceneComponent.h"
#include "Core/EngineTypes.h"
#include "Render/Proxy/DirtyFlag.h"

class ULightComponentBase : public USceneComponent
{
public:
	DECLARE_CLASS(ULightComponentBase, USceneComponent)

	// 가시성 토글 시 호출 — 위와 동일하되 Visibility dirty 플래그를 사용.
	void MarkRenderVisibilityDirty();
	void MarkProxyDirty(EDirtyFlag flag) const;

	//  Getter Setter Section
	float GetIntensity() const { return Intensity; }
	void SetIntensity(float NewIntensity) { Intensity = NewIntensity; }

	FLinearColor GetLightColor() const { return LightColor; }
	void SetLightColor(FLinearColor NewLightColor) { LightColor = NewLightColor; }

	bool IsVisible() const { return bVisible; }

	// Override
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;
	void Serialize(FArchive& Ar) override;

protected:
	float Intensity = 0.f;
	FLinearColor LightColor = FLinearColor(1.f, 1.f, 1.f, 1.f);
	bool bVisible = true;
};

	
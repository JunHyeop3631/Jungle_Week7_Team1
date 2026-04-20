#pragma once
#include "SceneComponent.h"
#include "Core/EngineTypes.h"
#include "Render/Proxy/DirtyFlag.h"

class FLightSceneProxy;

class ULightComponentBase : public USceneComponent
{
public:
	DECLARE_CLASS(ULightComponentBase, USceneComponent)

	// --- 렌더 상태 관리 ---
	void CreateRenderState() override;
	void DestroyRenderState() override;

	virtual FLightSceneProxy* CreateLightSceneProxy();
	// 가시성 토글 시 호출 — 위와 동일하되 Visibility dirty 플래그를 사용.
	void MarkRenderVisibilityDirty();
	void MarkProxyDirty(EDirtyFlag flag) const;
	void SetVisibility(bool bNewVisible);

	//  Getter Setter Section
	float GetIntensity() const { return Intensity; }
	void SetIntensity(float NewIntensity) { Intensity = NewIntensity; MarkProxyDirty(EDirtyFlag::LightData); }

	FLinearColor GetLightColor() const { return LightColor; }
	void SetLightColor(FLinearColor NewLightColor) { LightColor = NewLightColor; MarkProxyDirty(EDirtyFlag::LightData); }

	bool IsVisible() const { return bVisible; }

	// Override
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;
	void Serialize(FArchive& Ar) override;

protected:
	void OnTransformDirty() override;

	FLightSceneProxy* LightProxy = nullptr;
	float Intensity = 5.f;
	FLinearColor LightColor = FLinearColor(1.f, 1.f, 1.f, 1.f);
	bool bVisible = true;
};

	

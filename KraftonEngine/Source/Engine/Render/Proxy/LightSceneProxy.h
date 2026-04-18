#pragma once
#include "Core/EngineTypes.h"
#include "DirtyFlag.h"
#include "Math/Transform.h"
#include "Components/LightComponentBase.h"
#include "../Pipeline/RenderConstants.h"

class FRenderBus;
class UAmbientLightComponent;
class UDirectionalLightComponent;
class ULocalLightComponent;
class UPointLightComponent;
class USpotLightComponent;

// ============================================================
// FLightSceneProxy — lighting constants 빌드용 mirror
// ============================================================
// 컴포넌트 등록 시 CreateSceneProxy()로 1회 생성.
// 이후 DirtyFlags가 켜진 필드만 가상 함수를 통해 갱신.
// Light Constant를 갱신하는데 사용

struct FLightingBuildContext
{
	bool bHasAmbient = false;
	bool bHasDirectional = false;
};

class FLightSceneProxy
{
public:
	explicit FLightSceneProxy(ULightComponentBase* InComponent);
	virtual ~FLightSceneProxy() = default;

	// 가상 갱신 클래스
	virtual void UpdateTransform();
	virtual void UpdateVisibility();
	virtual void UpdateLightData();

	// 각 Component 별 상수 버퍼를 채우는 함수. 자식에서 Override해서 사용한다.
	virtual void CollectEntries(FLightingBuildContext& Context, FCollectedLightData& OutResult) {}
	
	// --- 식별 ---
	ULightComponentBase* Owner = nullptr;	// 소유 컴포넌트 (역참조용)
	uint32 ProxyId = UINT32_MAX;			// FScene내 Index
	bool bQueuedForDirtyUpdate = false;		// Dirty 갱신을 위한 Queue에 있다
	bool bVisible = true;

	// --- Dirty 관리 ---
	void MarkDirty(EDirtyFlag Flag) { DirtyFlags |= Flag; }
	void ClearDirty(EDirtyFlag Flag) { DirtyFlags &= ~Flag; }
	bool IsDirty(EDirtyFlag Flag) const { return HasFlag(DirtyFlags, Flag); }
	bool IsAnyDirty() const { return DirtyFlags != EDirtyFlag::None; }

	// --- 변경 추적 ---
	EDirtyFlag DirtyFlags = EDirtyFlag::All;

	// --- 캐싱된 조명 데이터 (등록 시 초기화, dirty 시만 갱신) ---
	FLinearColor CachedColor = FLinearColor(1.f, 1.f, 1.f, 1.f);
	float CachedIntensity = 0.0f;
	FTransform CachedTransform = {};
};

//======================================
// Ambient Light Scene Proxy
// - 해당 Proxy는 LightScene Proxy와 동일한 정보를 필요로 한다
//======================================
class FAmbientLightSceneProxy : public FLightSceneProxy
{
public:
	explicit FAmbientLightSceneProxy(UAmbientLightComponent* InComponent);

	virtual void CollectEntries(FLightingBuildContext& Context, FCollectedLightData& OutResult) override;

};

//======================================
// Directional Light Scene Proxy
// - 해당 Proxy는 LightScene Proxy와 동일한 정보를 필요로 한다.	
// - Direction 정보는 Transform에서 Rotation 정보를 사용한다
//======================================
class FDirectionalLightSceneProxy : public FLightSceneProxy
{
public:
	explicit FDirectionalLightSceneProxy(UDirectionalLightComponent* InComponent);

	virtual void CollectEntries(FLightingBuildContext& Context, FCollectedLightData& OutResult) override;

};

//======================================
// Local Light Scene Proxy
// - 해당 Proxy는 LightScene Proxy 정보에 추가로 AttenuationRadius를 필요로함.
// - 실제로 저장되는 Proxy는 아님. PointLight와 SpotLight프록시가 상속받는 부모 프록시
//======================================
class FLocalLightSceneProxy : public FLightSceneProxy
{
public:
	explicit FLocalLightSceneProxy(ULocalLightComponent* InComponent);

	void UpdateLightData() override;

	float CachedAttenuationRadius = 1.0f;
};

//======================================
// Point Light Scene Proxy
// - 해당 Proxy는 Local Light Scene Proxy에 추가로 FalloffExponent를 필요로함.
//======================================
class FPointLightSceneProxy : public FLocalLightSceneProxy
{
public:
	explicit FPointLightSceneProxy(UPointLightComponent* InComponent);

	void UpdateLightData() override;
	virtual void CollectEntries(FLightingBuildContext& Context, FCollectedLightData& OutResult) override;

	float CachedFalloffExponent = 1.0f;
};

//======================================
// Spot Light Scene Proxy
// - 해당 Proxy는 Local Light Scene Proxy에 추가로 Inner, Outer Angle을 필요로함.
//======================================
class FSpotLightSceneProxy : public FPointLightSceneProxy
{
public:
	explicit FSpotLightSceneProxy(USpotLightComponent* InComponent);

	void UpdateLightData() override;
	virtual void CollectEntries(FLightingBuildContext& Context, FCollectedLightData& OutResult) override;

	float CachedInnerConeAngle = 0.0f;
	float CachedOuterConeAngle = 30.0f;
};


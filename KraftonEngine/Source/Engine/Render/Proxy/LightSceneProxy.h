#pragma once
#include "Core/EngineTypes.h"
#include "DirtyFlag.h"
#include "Math/Transform.h"
#include "Components/LightComponentBase.h"

class FRenderBus;
// ============================================================
// FLightSceneProxy — lighting constants 빌드용 mirror
// ============================================================
// 컴포넌트 등록 시 CreateSceneProxy()로 1회 생성.
// 이후 DirtyFlags가 켜진 필드만 가상 함수를 통해 갱신.
// Light Constant를 갱신하는데 사용

enum class ELightType
{
	None,
	Ambient,
	Directional,
	Point,
	SpotLight,
	MAX
};

class FLightSceneProxy
{
public:
	FLightSceneProxy(ULightComponentBase* InComponent);
	virtual ~FLightSceneProxy() = default;

	// 가상 갱신 클래스
	virtual void UpdateTransform();
	virtual void UpdateVisibility();
	virtual void UpdateLightData();
	
	// --- 식별 ---
	ULightComponentBase* Owner = nullptr;	// 소유 컴포넌트 (역참조용)
	uint32 ProxyId = UINT32_MAX;			// FScene내 Index
	bool bQueuedForDirtyUpdate = false;		// Dirty 갱신을 위한 Queue에 있다
	bool bVisible = true;
	ELightType LightType = ELightType::None;

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


#include "Render/Proxy/FScene.h"
#include "Components/AmbientLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/LightComponentBase.h"
#include "Components/PointLightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneEffectSource.h"
#include "Components/SpotLightComponent.h"
#include "Object/Object.h"
#include "Render/Proxy/LightSceneProxy.h"
#include "Profiling/Stats.h"
#include <algorithm>
#include <cmath>

namespace
{
	void EnqueueDirtyProxy(TArray<FPrimitiveSceneProxy*>& DirtyList, FPrimitiveSceneProxy* Proxy)
	{
		if (!Proxy || Proxy->bQueuedForDirtyUpdate)
		{
			return;
		}

		Proxy->bQueuedForDirtyUpdate = true;
		DirtyList.push_back(Proxy);
	}

	void RemoveSelectedProxyFast(TArray<FPrimitiveSceneProxy*>& SelectedList, FPrimitiveSceneProxy* Proxy)
	{
		if (!Proxy || Proxy->SelectedListIndex == UINT32_MAX)
		{
			return;
		}

		const uint32 Index = Proxy->SelectedListIndex;
		const uint32 LastIndex = static_cast<uint32>(SelectedList.size() - 1);
		if (Index != LastIndex)
		{
			FPrimitiveSceneProxy* LastProxy = SelectedList.back();
			SelectedList[Index] = LastProxy;
			LastProxy->SelectedListIndex = Index;
		}

		SelectedList.pop_back();
		Proxy->SelectedListIndex = UINT32_MAX;
	}

	void EnqueueDirtyLightProxy(TArray<FLightSceneProxy*>& DirtyList, FLightSceneProxy* Proxy)
	{
		if (!Proxy || Proxy->bQueuedForDirtyUpdate)
		{
			return;
		}

		Proxy->bQueuedForDirtyUpdate = true;
		DirtyList.push_back(Proxy);
	}

	void AddUniqueFogComponent(TArray<UExponentialHeightFogComponent*>& FogList, UExponentialHeightFogComponent* Component)
	{
		if (!Component)
		{
			return;
		}

		if (std::find(FogList.begin(), FogList.end(), Component) == FogList.end())
		{
			FogList.push_back(Component);
		}
	}

	void RemoveFogComponent(TArray<UExponentialHeightFogComponent*>& FogList, UExponentialHeightFogComponent* Component)
	{
		if (!Component)
		{
			return;
		}

		auto It = std::find(FogList.begin(), FogList.end(), Component);
		if (It != FogList.end())
		{
			FogList.erase(It);
		}
	}

	FFogPostProcessConstants BuildFogPostProcessConstants(const TArray<UExponentialHeightFogComponent*>& FogList)
	{
		FFogPostProcessConstants Result = {};

		for (UExponentialHeightFogComponent* FogComponent : FogList)
		{
			if (!FogComponent || !FogComponent->IsFogActive())
			{
				continue;
			}

			Result.Fogs[0] = FogComponent->BuildFogUniformParameters();
			Result.FogCount = 1;
			return Result;
		}

		return Result;
	}

	// Light Proxy Helper
	bool ShouldWriteLight(const FLightSceneProxy* Proxy)
	{
		return Proxy && Proxy->bVisible && Proxy->CachedIntensity > 0.0f;
	}
}

// ============================================================
// 소멸자 — 모든 프록시 정리
// ============================================================
FScene::~FScene()
{
	for (FPrimitiveSceneProxy* Proxy : Proxies)
	{
		delete Proxy;
	}
	for (FLightSceneProxy* Proxy : LightProxies)
	{
		delete Proxy;
	}
	Proxies.clear();
	LightProxies.clear();
	DirtyProxies.clear();
	DirtyLightProxies.clear();
	SelectedProxies.clear();
	NeverCullProxies.clear();
	FreeSlots.clear();
	FreeLightSlots.clear();
	VisibleProxies.clear();
	SceneEffectSources.clear();
	FogComponents.clear();
}

// ============================================================
// AddPrimitive — Component의 CreateSceneProxy()로 구체 프록시 생성 후 등록
// ============================================================
// ============================================================
// RegisterProxy — 프록시를 슬롯에 배치하고 DirtyList에 추가
// ============================================================
void FScene::RegisterProxy(FPrimitiveSceneProxy* Proxy)
{
	if (!Proxy) return;

	Proxy->DirtyFlags = EDirtyFlag::All; // 초기 등록 시 전체 갱신

	// 빈 슬롯 재활용 또는 새 슬롯 할당
	if (!FreeSlots.empty())
	{
		uint32 Slot = FreeSlots.back();
		FreeSlots.pop_back();
		Proxy->ProxyId = Slot;
		Proxies[Slot] = Proxy;
	}
	else
	{
		Proxy->ProxyId = static_cast<uint32>(Proxies.size());
		Proxies.push_back(Proxy);
	}

	EnqueueDirtyProxy(DirtyProxies, Proxy);

	if (Proxy->bNeverCull)
		NeverCullProxies.push_back(Proxy);
}

FPrimitiveSceneProxy* FScene::AddPrimitive(UPrimitiveComponent* Component)
{
	if (!Component) return nullptr;

	// 컴포넌트가 자신에 맞는 구체 프록시를 생성 (다형성)
	FPrimitiveSceneProxy* Proxy = Component->CreateSceneProxy();
	if (!Proxy) return nullptr;

	RegisterProxy(Proxy);
	return Proxy;
}

FLightSceneProxy* FScene::AddLight(ULightComponentBase* Component)
{
	if (!Component)
	{
		return nullptr;
	}

	FLightSceneProxy* Proxy = Component->CreateLightSceneProxy();
	if (!Proxy)
	{
		return nullptr;
	}

	RegisterLightProxy(Proxy);
	return Proxy;
}

void FScene::RegisterLightProxy(FLightSceneProxy* Proxy)
{
	if (!Proxy)
	{
		return;
	}

	Proxy->DirtyFlags = EDirtyFlag::All;

	if (!FreeLightSlots.empty())
	{
		const uint32 Slot = FreeLightSlots.back();
		FreeLightSlots.pop_back();
		Proxy->ProxyId = Slot;
		LightProxies[Slot] = Proxy;
	}
	else
	{
		Proxy->ProxyId = static_cast<uint32>(LightProxies.size());
		LightProxies.push_back(Proxy);
	}

	EnqueueDirtyLightProxy(DirtyLightProxies, Proxy);
}

// ============================================================
// RemovePrimitive — 프록시 해제 및 슬롯 반환
// ============================================================
void FScene::RemovePrimitive(FPrimitiveSceneProxy* Proxy)
{
	if (!Proxy || Proxy->ProxyId == UINT32_MAX) return;

	uint32 Slot = Proxy->ProxyId;

	// 각 목록에서 제거
	if (Proxy->bQueuedForDirtyUpdate)
	{
		auto DirtyIt = std::find(DirtyProxies.begin(), DirtyProxies.end(), Proxy);
		if (DirtyIt != DirtyProxies.end())
		{
			*DirtyIt = DirtyProxies.back();
			DirtyProxies.pop_back();
		}
		Proxy->bQueuedForDirtyUpdate = false;
	}

	if (Proxy->SelectedListIndex != UINT32_MAX)
	{
		RemoveSelectedProxyFast(SelectedProxies, Proxy);
	}

	if (Proxy->bNeverCull)
	{
		auto it = std::find(NeverCullProxies.begin(), NeverCullProxies.end(), Proxy);
		if (it != NeverCullProxies.end()) NeverCullProxies.erase(it);
	}

	// VisibleProxies 캐시에서도 제거 — dangling 포인터 방지
	if (Proxy->bInVisibleSet && Proxy->VisibleListIndex < VisibleProxies.size())
	{
		const uint32 Index = Proxy->VisibleListIndex;
		const uint32 LastIndex = static_cast<uint32>(VisibleProxies.size() - 1);
		if (Index != LastIndex)
		{
			FPrimitiveSceneProxy* Last = VisibleProxies[LastIndex];
			VisibleProxies[Index] = Last;
			if (Last)
			{
				Last->VisibleListIndex = Index;
			}
		}
		VisibleProxies.pop_back();
	}
	bVisibleSetDirty = true;

	// 슬롯 비우고 재활용 목록에 추가
	Proxy->bInVisibleSet = false;
	Proxy->VisibleListIndex = UINT32_MAX;
	Proxies[Slot] = nullptr;
	FreeSlots.push_back(Slot);

	delete Proxy;
}

void FScene::RemoveLight(FLightSceneProxy* Proxy)
{
	if (!Proxy || Proxy->ProxyId == UINT32_MAX)
	{
		return;
	}

	const uint32 Slot = Proxy->ProxyId;
	if (Proxy->bQueuedForDirtyUpdate)
	{
		auto DirtyIt = std::find(DirtyLightProxies.begin(), DirtyLightProxies.end(), Proxy);
		if (DirtyIt != DirtyLightProxies.end())
		{
			*DirtyIt = DirtyLightProxies.back();
			DirtyLightProxies.pop_back();
		}
		Proxy->bQueuedForDirtyUpdate = false;
	}

	if (Slot < LightProxies.size())
	{
		LightProxies[Slot] = nullptr;
		FreeLightSlots.push_back(Slot);
	}

	delete Proxy;
}

// ============================================================
// UpdateDirtyProxies — 변경된 프록시만 갱신 (프레임당 1회)
// ============================================================
void FScene::UpdateDirtyProxies()
{
	SCOPE_STAT_CAT("UpdateDirtyProxies", "3_Collect");

	//Update 중 Transform/Mesh 업데이트가 다시 MarkProxyDirty를 호출할 수 있으므로
	//현재 배치는 스냅샷으로 분리해 순회한다.
	std::swap(DirtyProxies, ProcessingDirtyProxies);

	for (FPrimitiveSceneProxy* Proxy : ProcessingDirtyProxies)
	{
		if (!Proxy)
		{
			continue;
		}

		Proxy->bQueuedForDirtyUpdate = false;
		if (!Proxy->Owner) continue;

		// 현재 프레임에 처리할 dirty만 캡처하고, 처리 중 새로 발생한 dirty는
		// 다음 배치/다음 프레임에 남겨둔다.
		const EDirtyFlag FlagsToProcess = Proxy->DirtyFlags;
		Proxy->DirtyFlags = EDirtyFlag::None;

		// 가상 함수를 통해 서브클래스별 갱신 로직 호출
		if (HasFlag(FlagsToProcess, EDirtyFlag::Mesh))
		{
			Proxy->UpdateMesh();
		}
		else if (HasFlag(FlagsToProcess, EDirtyFlag::Material))
		{
			// Mesh가 이미 갱신됐으면 Material도 포함되므로 else if
			Proxy->UpdateMaterial();
		}

		if (HasFlag(FlagsToProcess, EDirtyFlag::Transform))
		{
			Proxy->UpdateTransform();
		}
		if (HasFlag(FlagsToProcess, EDirtyFlag::Visibility))
		{
			Proxy->UpdateVisibility();
		}
	}

	ProcessingDirtyProxies.clear();
}

void FScene::UpdateDirtyLightProxies()
{
	SCOPE_STAT_CAT("UpdateDirtyLightProxies", "3_Collect");

	std::swap(DirtyLightProxies, ProcessingDirtyLightProxies);

	for (FLightSceneProxy* Proxy : ProcessingDirtyLightProxies)
	{
		if (!Proxy)
		{
			continue;
		}

		Proxy->bQueuedForDirtyUpdate = false;
		if (!Proxy->Owner)
		{
			continue;
		}

		const EDirtyFlag FlagsToProcess = Proxy->DirtyFlags;
		Proxy->DirtyFlags = EDirtyFlag::None;

		if (HasFlag(FlagsToProcess, EDirtyFlag::Transform))
		{
			Proxy->UpdateTransform();
		}
		if (HasFlag(FlagsToProcess, EDirtyFlag::Visibility))
		{
			Proxy->UpdateVisibility();
		}
		if (HasFlag(FlagsToProcess, EDirtyFlag::LightData))
		{
			Proxy->UpdateLightData();
		}
	}

	ProcessingDirtyLightProxies.clear();
}

// ============================================================
// MarkProxyDirty — 외부에서 프록시의 특정 필드를 dirty로 마킹
// ============================================================
void FScene::MarkProxyDirty(FPrimitiveSceneProxy* Proxy, EDirtyFlag Flag)
{
	if (!Proxy) return;
	Proxy->MarkDirty(Flag);
	EnqueueDirtyProxy(DirtyProxies, Proxy);
}

void FScene::MarkLightProxyDirty(FLightSceneProxy* Proxy, EDirtyFlag Flag)
{
	if (!Proxy)
	{
		return;
	}

	Proxy->MarkDirty(Flag);
	EnqueueDirtyLightProxy(DirtyLightProxies, Proxy);
}

void FScene::MarkAllPerObjectCBDirty()
{
	for (FPrimitiveSceneProxy* Proxy : Proxies)
	{
		if (Proxy)
		{
			Proxy->MarkPerObjectCBDirty();
		}
	}
}

void FScene::RegisterSceneEffectSource(ISceneEffectSource* Source)
{
	if (!Source)
	{
		return;
	}

	if (std::find(SceneEffectSources.begin(), SceneEffectSources.end(), Source) == SceneEffectSources.end())
	{
		SceneEffectSources.push_back(Source);
	}
}

void FScene::UnregisterSceneEffectSource(ISceneEffectSource* Source)
{
	if (!Source)
	{
		return;
	}

	auto It = std::find(SceneEffectSources.begin(), SceneEffectSources.end(), Source);
	if (It != SceneEffectSources.end())
	{
		*It = SceneEffectSources.back();
		SceneEffectSources.pop_back();
	}
}

FSceneEffectConstants FScene::GetSceneEffectConstants() const
{
	FSceneEffectConstants Result = {};
	uint32 LocalTintIndex = 0;

	for (ISceneEffectSource* EffectSource : SceneEffectSources)
	{
		if (!EffectSource || !EffectSource->IsSceneEffectActive())
		{
			continue;
		}

		EffectSource->WriteSceneEffectConstants(Result, LocalTintIndex);
		++LocalTintIndex;

		if (LocalTintIndex >= ECBSlot::MaxLocalTintEffects)
		{
			break;
		}
	}

	Result.LocalTintCount = LocalTintIndex;

	return Result;
}

FLightingConstants FScene::GetLightingConstants() const
{
	FLightingConstants Result = {};

	/* 
	Light 개수 체크 목적의 Context
	bool bHasAmbient = false;
	bool bHasDirectional = false;
	uint32 PointCount = 0;
	uint32 SpotCount = 0;
	*/
	FLightingBuildContext Context = {};
	

	for (FLightSceneProxy* Proxy : LightProxies)
	{
		if (ShouldWriteLight(Proxy))
		{
			Proxy->CollectEntries(Context, Result);
		}
	}

	// LightSceneProxy의 BuildContext 결과를 기준으로 실제 존재하지 않는 라이트 채널은
	// 기본값(ambient/directional) 대신 0으로 강제해 "라이트 미배치 시 완전 암전" 동작을 보장한다.
	if (!Context.bHasAmbient)
	{
		Result.Ambient.LightColor = FVector4(0.0f, 0.0f, 0.0f, 1.0f);
	}

	if (!Context.bHasDirectional)
	{
		Result.Directional.LightColor = FVector4(0.0f, 0.0f, 0.0f, 1.0f);
	}

	return Result;
}

void FScene::RegisterFogComponent(UExponentialHeightFogComponent* Component)
{
	AddUniqueFogComponent(FogComponents, Component);
}

void FScene::UnregisterFogComponent(UExponentialHeightFogComponent* Component)
{
	RemoveFogComponent(FogComponents, Component);
}

FFogPostProcessConstants FScene::GetFogPostProcessConstants() const
{
	return BuildFogPostProcessConstants(FogComponents);
}

// ============================================================
// 선택 관리
// ============================================================
void FScene::SetProxySelected(FPrimitiveSceneProxy* Proxy, bool bSelected)
{
	if (!Proxy) return;
	Proxy->bSelected = bSelected;

	if (bSelected)
	{
		if (Proxy->SelectedListIndex == UINT32_MAX)
		{
			Proxy->SelectedListIndex = static_cast<uint32>(SelectedProxies.size());
			SelectedProxies.push_back(Proxy);
		}
	}
	else
	{
		RemoveSelectedProxyFast(SelectedProxies, Proxy);
	}
}

bool FScene::IsProxySelected(const FPrimitiveSceneProxy* Proxy) const
{
	return Proxy && Proxy->SelectedListIndex != UINT32_MAX;
}

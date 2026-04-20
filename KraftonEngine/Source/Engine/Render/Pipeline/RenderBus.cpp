#include "RenderBus.h"
#include "Components/CameraComponent.h"
#include "Viewport/Viewport.h"

void FRenderBus::Clear()
{
	for (uint32 i = 0; i < (uint32)ERenderPass::MAX; ++i)
	{
		ProxyQueues[i].clear();
	}

	FontEntries.clear();
	OverlayFontEntries.clear();
	SubUVEntries.clear();
	BillboardEntries.clear();
	AABBEntries.clear();
	OBBEntries.clear();
	GridEntries.clear();
	DebugLineEntries.clear();
	PointLights.clear();
	SpotLights.clear();
	SceneEffectConstants = {};
	FogPostProcessConstants = {};
	LightingConstants = {};

	ViewportRTV = nullptr;
	ViewportDSV = nullptr;
	ViewportSRV = nullptr;
	ViewportDepthSRV = nullptr;
	ViewportStencilSRV = nullptr;
}

void FRenderBus::AddProxy(ERenderPass Pass, const FPrimitiveSceneProxy* Proxy)
{
	ProxyQueues[(uint32)Pass].push_back(Proxy);
}

const TArray<const FPrimitiveSceneProxy*>& FRenderBus::GetProxies(ERenderPass Pass) const
{
	return ProxyQueues[(uint32)Pass];
}

void FRenderBus::AddFontEntry(FFontEntry&& Entry)
{
	FontEntries.push_back(std::move(Entry));
}

void FRenderBus::AddOverlayFontEntry(FFontEntry&& Entry)
{
	OverlayFontEntries.push_back(std::move(Entry));
}

void FRenderBus::AddSubUVEntry(FSubUVEntry&& Entry)
{
	SubUVEntries.push_back(std::move(Entry));
}

void FRenderBus::AddBillboardEntry(FBillboardEntry&& Entry)
{
	BillboardEntries.push_back(std::move(Entry));
}

void FRenderBus::AddAABBEntry(FAABBEntry&& Entry)
{
	AABBEntries.push_back(std::move(Entry));
}

void FRenderBus::AddOBBEntry(FOBBEntry&& Entry)
{
	OBBEntries.push_back(std::move(Entry));
}

void FRenderBus::AddGridEntry(FGridEntry&& Entry)
{
	GridEntries.push_back(std::move(Entry));
}

void FRenderBus::AddDebugLineEntry(FDebugLineEntry&& Entry)
{
	DebugLineEntries.push_back(std::move(Entry));
}

void FRenderBus::SetCameraInfo(const UCameraComponent* Camera)
{
	View = Camera->GetViewMatrix();
	Proj = Camera->GetProjectionMatrix();
	CameraPosition = Camera->GetWorldLocation();
	CameraForward = Camera->GetForwardVector();
	CameraRight = Camera->GetRightVector();
	CameraUp = Camera->GetUpVector();
	NearPlane = Camera->GetNearPlane();
	FarPlane = Camera->GetFarPlane();
	bIsOrtho = Camera->IsOrthogonal();
	OrthoWidth = Camera->GetOrthoWidth();

	const float NumSlices = 24.0f;
	const float SafeNear = std::max(NearPlane, 0.01f);
	if (bIsOrtho)
	{
		ClusterScale = NumSlices / (FarPlane - SafeNear);
		ClusterBias = -(NumSlices * SafeNear) / (FarPlane - SafeNear);
	}
	else
	{
		ClusterScale = NumSlices / std::log2(FarPlane / SafeNear);
		ClusterBias = -(NumSlices * std::log2(SafeNear)) / std::log2(FarPlane / SafeNear);
	}

	CachedConvexVolume.UpdateFromMatrix(View * Proj);
}

void FRenderBus::SetViewportInfo(const FViewport* VP)
{
	viewportWidth = static_cast<float>(VP->GetWidth());
	viewportHeight = static_cast<float>(VP->GetHeight());
	ViewportRTV = VP->GetRTV();
	ViewportDSV = VP->GetDSV();
	ViewportSRV = VP->GetSRV();
	ViewportDepthSRV = VP->GetDepthSRV();
	ViewportStencilSRV = VP->GetStencilSRV();
}

void FRenderBus::SetRenderSettings(const EViewMode NewViewMode, const FShowFlags NewShowFlags)
{
	ViewMode = NewViewMode;
	ShowFlags = NewShowFlags;
}

void FRenderBus::SetLightingData(const FCollectedLightData& InConstants)
{
	LightingConstants = InConstants.Constants;
	PointLights = std::move(InConstants.PointLights);
	SpotLights = std::move(InConstants.SpotLights);
}

void FRenderBus::SetViewportSize(float InWidth, float InHeight)
{
	viewportWidth = InWidth;
	viewportHeight = InHeight;
}

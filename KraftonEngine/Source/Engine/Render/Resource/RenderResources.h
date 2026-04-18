#pragma once
#include "Render/Resource/Buffer.h"
#include "Render/Pipeline/RenderConstants.h"

/*
	공용 Constant Buffer를 관리하는 구조체입니다.
	모든 커맨드가 공통으로 사용하는 Frame/PerObject CB만 소유합니다.
	타입별 CB(Gizmo, Editor, Outline 등)는 FConstantBufferPool에서 관리됩니다.
*/

struct FLightCullingBuffers
{
	// 라이트 데이터 원본 (CPU -> GPU, SRV만 필요)
	ID3D11Buffer* PointLightData = nullptr;
	ID3D11ShaderResourceView* PointLightDataSRV = nullptr;

	ID3D11Buffer* SpotLightData = nullptr;
	ID3D11ShaderResourceView* SpotLightDataSRV = nullptr;

	// Point Light 타일 컬링 결과 (CS에서 UAV 쓰기 -> PS에서 SRV 읽기)
	ID3D11Buffer* PointLightIndices = nullptr;
	ID3D11UnorderedAccessView* PointLightIndicesUAV = nullptr;
	ID3D11ShaderResourceView* PointLightIndicesSRV = nullptr;

	ID3D11Buffer* PointLightCounts = nullptr;
	ID3D11UnorderedAccessView* PointLightCountsUAV = nullptr;
	ID3D11ShaderResourceView* PointLightCountsSRV = nullptr;

	// Spot Light 타일 컬링 결과
	ID3D11Buffer* SpotLightIndices = nullptr;
	ID3D11UnorderedAccessView* SpotLightIndicesUAV = nullptr;
	ID3D11ShaderResourceView* SpotLightIndicesSRV = nullptr;

	ID3D11Buffer* SpotLightCounts = nullptr;
	ID3D11UnorderedAccessView* SpotLightCountsUAV = nullptr;
	ID3D11ShaderResourceView* SpotLightCountsSRV = nullptr;
};

struct FRenderResources
{
	FConstantBuffer FrameBuffer;				// b0 — ECBSlot::Frame
	FConstantBuffer PerObjectConstantBuffer;	// b1 — ECBSlot::PerObject
	FConstantBuffer SceneEffectBuffer;			// b5 — ECBSlot::SceneEffect
	ID3D11SamplerState* DefaultSampler = nullptr;	// s0 — Linear/Wrap
	FLightCullingBuffers LightCulling;

	void Create(ID3D11Device* InDevice);
	void CreateLightCullingBuffers(ID3D11Device* InDevice, uint32 ViewportWidth, uint32 ViewportHeight);
	void Release();
	void ReleaseLightCullingBuffers();
};

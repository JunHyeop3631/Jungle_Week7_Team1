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
	ID3D11Buffer* LocalLightData = nullptr;
	ID3D11ShaderResourceView* LocalLightDataSRV = nullptr;

	// 클러스터 기반 컬링 결과 버퍼
	ID3D11Buffer* LocalLightClusterGrid = nullptr; // Offset, Count 저장
	ID3D11UnorderedAccessView* LocalLightClusterGridUAV = nullptr;
	ID3D11ShaderResourceView* LocalLightClusterGridSRV = nullptr;

	ID3D11Buffer* LocalLightGlobalIndices = nullptr; // 실제 조명 인덱스가 담기는 배열
	ID3D11UnorderedAccessView* LocalLightGlobalIndicesUAV = nullptr;
	ID3D11ShaderResourceView* LocalLightGlobalIndicesSRV = nullptr;

	ID3D11Buffer* LocalLightGlobalCounter = nullptr; // 인덱스 할당용 카운터
	ID3D11UnorderedAccessView* LocalLightGlobalCounterUAV = nullptr;

	// Tile 기반 컬링 결과 버퍼 (Tiled Culling)
	ID3D11Buffer* LocalLightTileIndices = nullptr;
	ID3D11UnorderedAccessView* LocalLightTileIndicesUAV = nullptr;
	ID3D11ShaderResourceView* LocalLightTileIndicesSRV = nullptr;

	ID3D11Buffer* LocalLightTileCounts = nullptr;
	ID3D11UnorderedAccessView* LocalLightTileCountsUAV = nullptr;
	ID3D11ShaderResourceView* LocalLightTileCountsSRV = nullptr;
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

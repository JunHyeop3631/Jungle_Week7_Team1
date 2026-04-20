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

	// Point Light 클러스터 기반 컬링 결과 버퍼
	ID3D11Buffer* PointLightClusterGrid = nullptr; // Offset, Count 저장
	ID3D11UnorderedAccessView* PointLightClusterGridUAV = nullptr;
	ID3D11ShaderResourceView* PointLightClusterGridSRV = nullptr;

	ID3D11Buffer* PointLightGlobalIndices = nullptr; // 실제 조명 인덱스가 담기는 배열
	ID3D11UnorderedAccessView* PointLightGlobalIndicesUAV = nullptr;
	ID3D11ShaderResourceView* PointLightGlobalIndicesSRV = nullptr;

	ID3D11Buffer* PointLightGlobalCounter = nullptr; // 인덱스 할당용 카운터
	ID3D11UnorderedAccessView* PointLightGlobalCounterUAV = nullptr;

	// Spot Light 클러스터 기반 컬링 결과 버퍼
	ID3D11Buffer* SpotLightClusterGrid = nullptr; // Offset, Count 저장
	ID3D11UnorderedAccessView* SpotLightClusterGridUAV = nullptr;
	ID3D11ShaderResourceView* SpotLightClusterGridSRV = nullptr;

	ID3D11Buffer* SpotLightGlobalIndices = nullptr; // 실제 조명 인덱스가 담기는 배열
	ID3D11UnorderedAccessView* SpotLightGlobalIndicesUAV = nullptr;
	ID3D11ShaderResourceView* SpotLightGlobalIndicesSRV = nullptr;

	ID3D11Buffer* SpotLightGlobalCounter = nullptr; // 인덱스 할당용 카운터
	ID3D11UnorderedAccessView* SpotLightGlobalCounterUAV = nullptr;
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

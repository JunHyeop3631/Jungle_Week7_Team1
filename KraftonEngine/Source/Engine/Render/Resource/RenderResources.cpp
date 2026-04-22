#include "RenderResources.h"

namespace
{
	template <typename T>
	void SafeRelease(T*& Resource)
	{
		if (Resource)
		{
			Resource->Release();
			Resource = nullptr;
		}
	}

	// 최대 처리 가능한 라이트 개수 (필요에 따라 늘리거나 줄이세요)
	constexpr uint32 MAX_CULLING_LIGHTS = 5000;
}

void FRenderResources::Create(ID3D11Device* InDevice)
{
	FrameBuffer.Create(InDevice, sizeof(FFrameConstants));
	PerObjectConstantBuffer.Create(InDevice, sizeof(FPerObjectConstants));
	SceneEffectBuffer.Create(InDevice, sizeof(FSceneEffectConstants));

	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	InDevice->CreateSamplerState(&sampDesc, &DefaultSampler);
}

void FRenderResources::CreateLightCullingBuffers(ID3D11Device* InDevice, uint32 ViewportWidth, uint32 ViewportHeight)
{
	ReleaseLightCullingBuffers();

	if (ViewportWidth == 0 || ViewportHeight == 0) return;

	const uint32 TILE_SIZE = 16;
	const uint32 CLUSTER_SLICES = 24;

	// 전체 씬에서 허용할 수 있는 조명 교차(장바구니) 개수
	const uint32 MAX_GLOBAL_LIGHT_INDICES = 2000000;

	uint32 NumTilesX = (ViewportWidth + TILE_SIZE - 1) / TILE_SIZE;
	uint32 NumTilesY = (ViewportHeight + TILE_SIZE - 1) / TILE_SIZE;

	uint32 TotalClusters = NumTilesX * NumTilesY * CLUSTER_SLICES;


	// 원본 라이트 데이터
	D3D11_BUFFER_DESC dataDesc = {};
	dataDesc.Usage = D3D11_USAGE_DYNAMIC;
	dataDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	dataDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	dataDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

	D3D11_SHADER_RESOURCE_VIEW_DESC dataSrvDesc = {};
	dataSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
	dataSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	dataSrvDesc.Buffer.FirstElement = 0;
	dataSrvDesc.Buffer.NumElements = MAX_CULLING_LIGHTS;

	// Light Data
	dataDesc.ByteWidth = sizeof(FLightData) * MAX_CULLING_LIGHTS;
	dataDesc.StructureByteStride = sizeof(FLightData);
	InDevice->CreateBuffer(&dataDesc, nullptr, &LightCulling.LocalLightData);
	InDevice->CreateShaderResourceView(LightCulling.LocalLightData, &dataSrvDesc, &LightCulling.LocalLightDataSRV);


	// === [2. GPU 컬링 결과 저장용 공통 Desc 세팅] ===
	D3D11_BUFFER_DESC bufDesc = {};
	bufDesc.Usage = D3D11_USAGE_DEFAULT;
	bufDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	bufDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;

	// 클러스터 결과 버퍼 생성

	// 2-1. Cluster Grid (uint2: Offset, Count)
	bufDesc.StructureByteStride = sizeof(uint32) * 2;
	bufDesc.ByteWidth = TotalClusters * bufDesc.StructureByteStride;
	InDevice->CreateBuffer(&bufDesc, nullptr, &LightCulling.LocalLightClusterGrid);
	uavDesc.Buffer.NumElements = srvDesc.Buffer.NumElements = TotalClusters;
	InDevice->CreateUnorderedAccessView(LightCulling.LocalLightClusterGrid, &uavDesc, &LightCulling.LocalLightClusterGridUAV);
	InDevice->CreateShaderResourceView(LightCulling.LocalLightClusterGrid, &srvDesc, &LightCulling.LocalLightClusterGridSRV);

	// 2-2. Global Indices (uint)
	bufDesc.StructureByteStride = sizeof(uint32);
	bufDesc.ByteWidth = MAX_GLOBAL_LIGHT_INDICES * bufDesc.StructureByteStride;
	InDevice->CreateBuffer(&bufDesc, nullptr, &LightCulling.LocalLightGlobalIndices);
	uavDesc.Buffer.NumElements = srvDesc.Buffer.NumElements = MAX_GLOBAL_LIGHT_INDICES;
	InDevice->CreateUnorderedAccessView(LightCulling.LocalLightGlobalIndices, &uavDesc, &LightCulling.LocalLightGlobalIndicesUAV);
	InDevice->CreateShaderResourceView(LightCulling.LocalLightGlobalIndices, &srvDesc, &LightCulling.LocalLightGlobalIndicesSRV);

	// 2-3. Global Counter (uint, 1칸짜리, SRV는 필요 없음)
	bufDesc.ByteWidth = sizeof(uint32);
	bufDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS; // 카운터는 SRV로 안 읽음
	InDevice->CreateBuffer(&bufDesc, nullptr, &LightCulling.LocalLightGlobalCounter);
	uavDesc.Buffer.NumElements = 1;
	InDevice->CreateUnorderedAccessView(LightCulling.LocalLightGlobalCounter, &uavDesc, &LightCulling.LocalLightGlobalCounterUAV);

	// Tile Based 결과 버퍼 생성
	const uint32 MAX_LIGHTS_PER_TILE = 256;
	uint32 TotalTiles = NumTilesX * NumTilesY;

	bufDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;

	// Tile Counts
	bufDesc.StructureByteStride = sizeof(uint32);
	bufDesc.ByteWidth = TotalTiles * bufDesc.StructureByteStride;
	InDevice->CreateBuffer(&bufDesc, nullptr, &LightCulling.LocalLightTileCounts);
	uavDesc.Buffer.NumElements = srvDesc.Buffer.NumElements = TotalTiles;
	InDevice->CreateUnorderedAccessView(LightCulling.LocalLightTileCounts, &uavDesc, &LightCulling.LocalLightTileCountsUAV);
	InDevice->CreateShaderResourceView(LightCulling.LocalLightTileCounts, &srvDesc, &LightCulling.LocalLightTileCountsSRV);

	// Point Light Tile Indices
	bufDesc.ByteWidth = TotalTiles * MAX_LIGHTS_PER_TILE * bufDesc.StructureByteStride;
	InDevice->CreateBuffer(&bufDesc, nullptr, &LightCulling.LocalLightTileIndices);
	uavDesc.Buffer.NumElements = srvDesc.Buffer.NumElements = TotalTiles * MAX_LIGHTS_PER_TILE;
	InDevice->CreateUnorderedAccessView(LightCulling.LocalLightTileIndices, &uavDesc, &LightCulling.LocalLightTileIndicesUAV);
	InDevice->CreateShaderResourceView(LightCulling.LocalLightTileIndices, &srvDesc, &LightCulling.LocalLightTileIndicesSRV);

}

void FRenderResources::ReleaseLightCullingBuffers()
{
	// Point Light 원본 데이터
	SafeRelease(LightCulling.LocalLightDataSRV);
	SafeRelease(LightCulling.LocalLightData);

	// Point Light 클러스터 결과
	SafeRelease(LightCulling.LocalLightClusterGridSRV);
	SafeRelease(LightCulling.LocalLightClusterGridUAV);
	SafeRelease(LightCulling.LocalLightClusterGrid);

	SafeRelease(LightCulling.LocalLightGlobalIndicesSRV);
	SafeRelease(LightCulling.LocalLightGlobalIndicesUAV);
	SafeRelease(LightCulling.LocalLightGlobalIndices);

	SafeRelease(LightCulling.LocalLightGlobalCounterUAV);
	SafeRelease(LightCulling.LocalLightGlobalCounter);

	// Tile 기반 결과
	SafeRelease(LightCulling.LocalLightTileIndicesSRV);
	SafeRelease(LightCulling.LocalLightTileIndicesUAV);
	SafeRelease(LightCulling.LocalLightTileIndices);

	SafeRelease(LightCulling.LocalLightTileCountsSRV);
	SafeRelease(LightCulling.LocalLightTileCountsUAV);
	SafeRelease(LightCulling.LocalLightTileCounts);
}

void FRenderResources::Release()
{
	FrameBuffer.Release();
	PerObjectConstantBuffer.Release();
	SceneEffectBuffer.Release();
	if (DefaultSampler) { DefaultSampler->Release(); DefaultSampler = nullptr; }

	ReleaseLightCullingBuffers();
}

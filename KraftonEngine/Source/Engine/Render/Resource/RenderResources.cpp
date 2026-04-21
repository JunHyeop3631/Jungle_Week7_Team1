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

	// 전체 씬에서 Point/Spot 각각 허용할 수 있는 최대 조명 교차(장바구니) 개수
	// 50만개면 메모리도 적게 먹으면서 오버플로우 걱정이 없는 아주 넉넉한 수치입니다.
	const uint32 MAX_GLOBAL_LIGHT_INDICES = 2000000;

	uint32 NumTilesX = (ViewportWidth + TILE_SIZE - 1) / TILE_SIZE;
	uint32 NumTilesY = (ViewportHeight + TILE_SIZE - 1) / TILE_SIZE;

	uint32 TotalClusters = NumTilesX * NumTilesY * CLUSTER_SLICES;


	// === [1. 원본 라이트 데이터 (Point / Spot)] ===
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

	// Point Light Data
	dataDesc.ByteWidth = sizeof(FPointLightInfo) * MAX_CULLING_LIGHTS;
	dataDesc.StructureByteStride = sizeof(FPointLightInfo);
	InDevice->CreateBuffer(&dataDesc, nullptr, &LightCulling.PointLightData);
	InDevice->CreateShaderResourceView(LightCulling.PointLightData, &dataSrvDesc, &LightCulling.PointLightDataSRV);

	// Spot Light Data
	dataDesc.ByteWidth = sizeof(FSpotLightInfo) * MAX_CULLING_LIGHTS;
	dataDesc.StructureByteStride = sizeof(FSpotLightInfo);
	InDevice->CreateBuffer(&dataDesc, nullptr, &LightCulling.SpotLightData);
	InDevice->CreateShaderResourceView(LightCulling.SpotLightData, &dataSrvDesc, &LightCulling.SpotLightDataSRV);


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

	// =========================================================
	// [Point Light] 클러스터 결과 버퍼 생성
	// =========================================================

	// 2-1. Cluster Grid (uint2: Offset, Count)
	bufDesc.StructureByteStride = sizeof(uint32) * 2;
	bufDesc.ByteWidth = TotalClusters * bufDesc.StructureByteStride;
	InDevice->CreateBuffer(&bufDesc, nullptr, &LightCulling.PointLightClusterGrid);
	uavDesc.Buffer.NumElements = srvDesc.Buffer.NumElements = TotalClusters;
	InDevice->CreateUnorderedAccessView(LightCulling.PointLightClusterGrid, &uavDesc, &LightCulling.PointLightClusterGridUAV);
	InDevice->CreateShaderResourceView(LightCulling.PointLightClusterGrid, &srvDesc, &LightCulling.PointLightClusterGridSRV);

	// 2-2. Global Indices (uint)
	bufDesc.StructureByteStride = sizeof(uint32);
	bufDesc.ByteWidth = MAX_GLOBAL_LIGHT_INDICES * bufDesc.StructureByteStride;
	InDevice->CreateBuffer(&bufDesc, nullptr, &LightCulling.PointLightGlobalIndices);
	uavDesc.Buffer.NumElements = srvDesc.Buffer.NumElements = MAX_GLOBAL_LIGHT_INDICES;
	InDevice->CreateUnorderedAccessView(LightCulling.PointLightGlobalIndices, &uavDesc, &LightCulling.PointLightGlobalIndicesUAV);
	InDevice->CreateShaderResourceView(LightCulling.PointLightGlobalIndices, &srvDesc, &LightCulling.PointLightGlobalIndicesSRV);

	// 2-3. Global Counter (uint, 1칸짜리, SRV는 필요 없음)
	bufDesc.ByteWidth = sizeof(uint32);
	bufDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS; // 카운터는 SRV로 안 읽음
	InDevice->CreateBuffer(&bufDesc, nullptr, &LightCulling.PointLightGlobalCounter);
	uavDesc.Buffer.NumElements = 1;
	InDevice->CreateUnorderedAccessView(LightCulling.PointLightGlobalCounter, &uavDesc, &LightCulling.PointLightGlobalCounterUAV);


	// =========================================================
	// [Spot Light] 클러스터 결과 버퍼 생성
	// =========================================================

	bufDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE; // SRV 플래그 복구

	// 2-1. Cluster Grid (uint2: Offset, Count)
	bufDesc.StructureByteStride = sizeof(uint32) * 2;
	bufDesc.ByteWidth = TotalClusters * bufDesc.StructureByteStride;
	InDevice->CreateBuffer(&bufDesc, nullptr, &LightCulling.SpotLightClusterGrid);
	uavDesc.Buffer.NumElements = srvDesc.Buffer.NumElements = TotalClusters;
	InDevice->CreateUnorderedAccessView(LightCulling.SpotLightClusterGrid, &uavDesc, &LightCulling.SpotLightClusterGridUAV);
	InDevice->CreateShaderResourceView(LightCulling.SpotLightClusterGrid, &srvDesc, &LightCulling.SpotLightClusterGridSRV);

	// 2-2. Global Indices (uint)
	bufDesc.StructureByteStride = sizeof(uint32);
	bufDesc.ByteWidth = MAX_GLOBAL_LIGHT_INDICES * bufDesc.StructureByteStride;
	InDevice->CreateBuffer(&bufDesc, nullptr, &LightCulling.SpotLightGlobalIndices);
	uavDesc.Buffer.NumElements = srvDesc.Buffer.NumElements = MAX_GLOBAL_LIGHT_INDICES;
	InDevice->CreateUnorderedAccessView(LightCulling.SpotLightGlobalIndices, &uavDesc, &LightCulling.SpotLightGlobalIndicesUAV);
	InDevice->CreateShaderResourceView(LightCulling.SpotLightGlobalIndices, &srvDesc, &LightCulling.SpotLightGlobalIndicesSRV);

	// 2-3. Global Counter (uint, 1칸짜리, SRV는 필요 없음)
	bufDesc.ByteWidth = sizeof(uint32);
	bufDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	InDevice->CreateBuffer(&bufDesc, nullptr, &LightCulling.SpotLightGlobalCounter);
	uavDesc.Buffer.NumElements = 1;
	InDevice->CreateUnorderedAccessView(LightCulling.SpotLightGlobalCounter, &uavDesc, &LightCulling.SpotLightGlobalCounterUAV);

	// =========================================================
	// [Tile Based] 결과 버퍼 생성
	// =========================================================
	const uint32 MAX_LIGHTS_PER_TILE = 256;
	uint32 TotalTiles = NumTilesX * NumTilesY;

	bufDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;

	// Point Light Tile Counts
	bufDesc.StructureByteStride = sizeof(uint32);
	bufDesc.ByteWidth = TotalTiles * bufDesc.StructureByteStride;
	InDevice->CreateBuffer(&bufDesc, nullptr, &LightCulling.PointLightTileCounts);
	uavDesc.Buffer.NumElements = srvDesc.Buffer.NumElements = TotalTiles;
	InDevice->CreateUnorderedAccessView(LightCulling.PointLightTileCounts, &uavDesc, &LightCulling.PointLightTileCountsUAV);
	InDevice->CreateShaderResourceView(LightCulling.PointLightTileCounts, &srvDesc, &LightCulling.PointLightTileCountsSRV);

	// Point Light Tile Indices
	bufDesc.ByteWidth = TotalTiles * MAX_LIGHTS_PER_TILE * bufDesc.StructureByteStride;
	InDevice->CreateBuffer(&bufDesc, nullptr, &LightCulling.PointLightTileIndices);
	uavDesc.Buffer.NumElements = srvDesc.Buffer.NumElements = TotalTiles * MAX_LIGHTS_PER_TILE;
	InDevice->CreateUnorderedAccessView(LightCulling.PointLightTileIndices, &uavDesc, &LightCulling.PointLightTileIndicesUAV);
	InDevice->CreateShaderResourceView(LightCulling.PointLightTileIndices, &srvDesc, &LightCulling.PointLightTileIndicesSRV);

	// Spot Light Tile Counts
	bufDesc.ByteWidth = TotalTiles * bufDesc.StructureByteStride;
	InDevice->CreateBuffer(&bufDesc, nullptr, &LightCulling.SpotLightTileCounts);
	uavDesc.Buffer.NumElements = srvDesc.Buffer.NumElements = TotalTiles;
	InDevice->CreateUnorderedAccessView(LightCulling.SpotLightTileCounts, &uavDesc, &LightCulling.SpotLightTileCountsUAV);
	InDevice->CreateShaderResourceView(LightCulling.SpotLightTileCounts, &srvDesc, &LightCulling.SpotLightTileCountsSRV);

	// Spot Light Tile Indices
	bufDesc.ByteWidth = TotalTiles * MAX_LIGHTS_PER_TILE * bufDesc.StructureByteStride;
	InDevice->CreateBuffer(&bufDesc, nullptr, &LightCulling.SpotLightTileIndices);
	uavDesc.Buffer.NumElements = srvDesc.Buffer.NumElements = TotalTiles * MAX_LIGHTS_PER_TILE;
	InDevice->CreateUnorderedAccessView(LightCulling.SpotLightTileIndices, &uavDesc, &LightCulling.SpotLightTileIndicesUAV);
	InDevice->CreateShaderResourceView(LightCulling.SpotLightTileIndices, &srvDesc, &LightCulling.SpotLightTileIndicesSRV);
}

void FRenderResources::ReleaseLightCullingBuffers()
{
	// Point Light 원본 데이터
	SafeRelease(LightCulling.PointLightDataSRV);
	SafeRelease(LightCulling.PointLightData);

	// Point Light 클러스터 결과
	SafeRelease(LightCulling.PointLightClusterGridSRV);
	SafeRelease(LightCulling.PointLightClusterGridUAV);
	SafeRelease(LightCulling.PointLightClusterGrid);

	SafeRelease(LightCulling.PointLightGlobalIndicesSRV);
	SafeRelease(LightCulling.PointLightGlobalIndicesUAV);
	SafeRelease(LightCulling.PointLightGlobalIndices);

	SafeRelease(LightCulling.PointLightGlobalCounterUAV);
	SafeRelease(LightCulling.PointLightGlobalCounter);

	// Spot Light 원본 데이터
	SafeRelease(LightCulling.SpotLightDataSRV);
	SafeRelease(LightCulling.SpotLightData);

	// Spot Light 클러스터 결과
	SafeRelease(LightCulling.SpotLightClusterGridSRV);
	SafeRelease(LightCulling.SpotLightClusterGridUAV);
	SafeRelease(LightCulling.SpotLightClusterGrid);

	SafeRelease(LightCulling.SpotLightGlobalIndicesSRV);
	SafeRelease(LightCulling.SpotLightGlobalIndicesUAV);
	SafeRelease(LightCulling.SpotLightGlobalIndices);

	SafeRelease(LightCulling.SpotLightGlobalCounterUAV);
	SafeRelease(LightCulling.SpotLightGlobalCounter);

	// Tile 기반 결과
	SafeRelease(LightCulling.PointLightTileIndicesSRV);
	SafeRelease(LightCulling.PointLightTileIndicesUAV);
	SafeRelease(LightCulling.PointLightTileIndices);

	SafeRelease(LightCulling.PointLightTileCountsSRV);
	SafeRelease(LightCulling.PointLightTileCountsUAV);
	SafeRelease(LightCulling.PointLightTileCounts);

	SafeRelease(LightCulling.SpotLightTileIndicesSRV);
	SafeRelease(LightCulling.SpotLightTileIndicesUAV);
	SafeRelease(LightCulling.SpotLightTileIndices);

	SafeRelease(LightCulling.SpotLightTileCountsSRV);
	SafeRelease(LightCulling.SpotLightTileCountsUAV);
	SafeRelease(LightCulling.SpotLightTileCounts);
}

void FRenderResources::Release()
{
	FrameBuffer.Release();
	PerObjectConstantBuffer.Release();
	SceneEffectBuffer.Release();
	if (DefaultSampler) { DefaultSampler->Release(); DefaultSampler = nullptr; }

	ReleaseLightCullingBuffers();
}

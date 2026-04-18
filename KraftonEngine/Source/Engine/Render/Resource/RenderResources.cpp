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
	constexpr uint32 MAX_CULLING_LIGHTS = 1024;
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
	const uint32 MAX_LIGHTS_PER_TILE = 64;

	uint32 NumTilesX = (ViewportWidth + TILE_SIZE - 1) / TILE_SIZE;
	uint32 NumTilesY = (ViewportHeight + TILE_SIZE - 1) / TILE_SIZE;
	uint32 TotalTiles = NumTilesX * NumTilesY;


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


	// 2. GPU 컬링 결과 저장용 버퍼 세팅 (Indices, Counts) / DEFAULT & UAV & SRV
	D3D11_BUFFER_DESC indicesDesc = {};
	indicesDesc.Usage = D3D11_USAGE_DEFAULT;
	indicesDesc.ByteWidth = TotalTiles * MAX_LIGHTS_PER_TILE * sizeof(uint32);
	indicesDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	indicesDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	indicesDesc.StructureByteStride = sizeof(uint32);

	D3D11_BUFFER_DESC countsDesc = indicesDesc;
	countsDesc.ByteWidth = TotalTiles * sizeof(uint32);

	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;

	// --- [Point Light] Indices & Counts ---
	InDevice->CreateBuffer(&indicesDesc, nullptr, &LightCulling.PointLightIndices);
	uavDesc.Buffer.NumElements = srvDesc.Buffer.NumElements = TotalTiles * MAX_LIGHTS_PER_TILE;
	InDevice->CreateUnorderedAccessView(LightCulling.PointLightIndices, &uavDesc, &LightCulling.PointLightIndicesUAV);
	InDevice->CreateShaderResourceView(LightCulling.PointLightIndices, &srvDesc, &LightCulling.PointLightIndicesSRV);

	InDevice->CreateBuffer(&countsDesc, nullptr, &LightCulling.PointLightCounts);
	uavDesc.Buffer.NumElements = srvDesc.Buffer.NumElements = TotalTiles;
	InDevice->CreateUnorderedAccessView(LightCulling.PointLightCounts, &uavDesc, &LightCulling.PointLightCountsUAV);
	InDevice->CreateShaderResourceView(LightCulling.PointLightCounts, &srvDesc, &LightCulling.PointLightCountsSRV);

	// --- [Spot Light] Indices & Counts ---
	InDevice->CreateBuffer(&indicesDesc, nullptr, &LightCulling.SpotLightIndices);
	uavDesc.Buffer.NumElements = srvDesc.Buffer.NumElements = TotalTiles * MAX_LIGHTS_PER_TILE;
	InDevice->CreateUnorderedAccessView(LightCulling.SpotLightIndices, &uavDesc, &LightCulling.SpotLightIndicesUAV);
	InDevice->CreateShaderResourceView(LightCulling.SpotLightIndices, &srvDesc, &LightCulling.SpotLightIndicesSRV);

	InDevice->CreateBuffer(&countsDesc, nullptr, &LightCulling.SpotLightCounts);
	uavDesc.Buffer.NumElements = srvDesc.Buffer.NumElements = TotalTiles;
	InDevice->CreateUnorderedAccessView(LightCulling.SpotLightCounts, &uavDesc, &LightCulling.SpotLightCountsUAV);
	InDevice->CreateShaderResourceView(LightCulling.SpotLightCounts, &srvDesc, &LightCulling.SpotLightCountsSRV);
}

void FRenderResources::ReleaseLightCullingBuffers()
{
	// Point Light 릴리즈
	SafeRelease(LightCulling.PointLightDataSRV);
	SafeRelease(LightCulling.PointLightData);
	SafeRelease(LightCulling.PointLightIndicesSRV);
	SafeRelease(LightCulling.PointLightIndicesUAV);
	SafeRelease(LightCulling.PointLightIndices);
	SafeRelease(LightCulling.PointLightCountsSRV);
	SafeRelease(LightCulling.PointLightCountsUAV);
	SafeRelease(LightCulling.PointLightCounts);

	// Spot Light 릴리즈
	SafeRelease(LightCulling.SpotLightDataSRV);
	SafeRelease(LightCulling.SpotLightData);
	SafeRelease(LightCulling.SpotLightIndicesSRV);
	SafeRelease(LightCulling.SpotLightIndicesUAV);
	SafeRelease(LightCulling.SpotLightIndices);
	SafeRelease(LightCulling.SpotLightCountsSRV);
	SafeRelease(LightCulling.SpotLightCountsUAV);
	SafeRelease(LightCulling.SpotLightCounts);
}

void FRenderResources::Release()
{
	FrameBuffer.Release();
	PerObjectConstantBuffer.Release();
	SceneEffectBuffer.Release();
	if (DefaultSampler) { DefaultSampler->Release(); DefaultSampler = nullptr; }

	ReleaseLightCullingBuffers();
}

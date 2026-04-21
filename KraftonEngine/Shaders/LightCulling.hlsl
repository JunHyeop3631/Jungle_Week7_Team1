#include "Common/Functions.hlsl"
#include "Common/VertexLayouts.hlsl"
#include "Common/ConstantBuffers.hlsl"

// SRV
//StructuredBuffer<FLightData> g_Lights : register(t0);
Texture2D<float> DepthTexture : register(t1);

// UAV
RWStructuredBuffer<uint2> ClusterGrid : register(u0);
RWStructuredBuffer<uint> GlobalIndices : register(u1);
RWStructuredBuffer<uint> GlobalCounts: register(u2);

#define TILE_SIZE 16
#define CLUSTER_SLICES 24
#define MAX_LIGHTS_PER_CLUSTER 256
#define MAX_GLOBAL_LIGHT_INDICES 2000000

groupshared uint g_MinDepthInt;
groupshared uint g_MaxDepthInt;

groupshared uint g_ClusterLightCount[CLUSTER_SLICES];
groupshared uint g_ClusterStartOffset[CLUSTER_SLICES];
groupshared uint g_ClusterLightIndices[CLUSTER_SLICES][MAX_LIGHTS_PER_CLUSTER];

struct Plane
{
    float3 Normal;
    float DistanceToOrigin;
};
groupshared Plane g_FrustumPlanes[4];

float3 ScreenToView(float4 screenPos, float2 screenDims)
{
    float2 texCoord = screenPos.xy / screenDims;
    float4 clip = float4(texCoord.x * 2.0f - 1.0f, (1.0f - texCoord.y) * 2.0f - 1.0f, screenPos.z, screenPos.w);
    float4 view = mul(clip, InverseProjection);
    return view.xyz / view.w;
}

uint GetZSlice(float viewZ)
{
	// CPU선에서 ClusterScale과 ClusterBias를 계산하여 log(A / B) = log(A) - log(B)공식을 적용시킨 계산식임.
    int slice = (int) (log2(viewZ) * ClusterScale + ClusterBias);
    return (uint) clamp(slice, 0, CLUSTER_SLICES - 1);
}

void InitializeTileAndFrustum(uint3 groupId, uint3 dispatchThreadId, uint groupIndex, uint screenWidth, uint screenHeight)
{
    if (groupIndex == 0)
    {
        g_MinDepthInt = 0x7f7fffff;
        g_MaxDepthInt = 0;
    }

    // 16x16 스레드 중 24개가 각 층의 카운터 0으로 초기화
    if (groupIndex < CLUSTER_SLICES)
    {
        g_ClusterLightCount[groupIndex] = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    // 16x16에서의 ViewZ 계산. -> 해당 픽셀의 Z값 계산
    float viewZ = FarPlane;
    if (dispatchThreadId.x < screenWidth && dispatchThreadId.y < screenHeight)
    {
        float depth = DepthTexture.Load(int3(dispatchThreadId.xy, 0)).r;
        if (depth < 1.0f)
        {
            float4 clip = float4(0.0f, 0.0f, depth, 1.0f);
            float4 view = mul(clip, InverseProjection);
            viewZ = view.z / view.w;
        }
    }

    // minDepth, maxDepth 계산.
    uint zInt = asuint(viewZ);
    InterlockedMin(g_MinDepthInt, zInt);
    InterlockedMax(g_MaxDepthInt, zInt);
    GroupMemoryBarrierWithGroupSync();

    // minDepth, maxDepth기반 절두체의 normal 계산 -> 빛의 원의 거리 계산에 사용
    if (groupIndex == 0)
    {
        float2 screenDims = float2((float) screenWidth, (float) screenHeight);
        uint2 tileMin = groupId.xy * TILE_SIZE;
        uint2 tileMax = tileMin + TILE_SIZE;

       // Far 평면의 4꼭짓점
        float3 vBL_Far = ScreenToView(float4(tileMin.x, tileMax.y, 1.0f, 1.0f), screenDims);
        float3 vTL_Far = ScreenToView(float4(tileMin.x, tileMin.y, 1.0f, 1.0f), screenDims);
        float3 vTR_Far = ScreenToView(float4(tileMax.x, tileMin.y, 1.0f, 1.0f), screenDims);
        float3 vBR_Far = ScreenToView(float4(tileMax.x, tileMax.y, 1.0f, 1.0f), screenDims);

		// Near 평면의 4꼭짓점
        float3 vBL_Near = ScreenToView(float4(tileMin.x, tileMax.y, 0.0f, 1.0f), screenDims);
        float3 vTL_Near = ScreenToView(float4(tileMin.x, tileMin.y, 0.0f, 1.0f), screenDims);
        float3 vTR_Near = ScreenToView(float4(tileMax.x, tileMin.y, 0.0f, 1.0f), screenDims);
        float3 vBR_Near = ScreenToView(float4(tileMax.x, tileMax.y, 0.0f, 1.0f), screenDims);
        
        float3 viewCenter = ScreenToView(float4((tileMin.x + tileMax.x) * 0.5f, (tileMin.y + tileMax.y) * 0.5f, 1.0f, 1.0f), screenDims);

        // 3개의 점(벡터)을 외적하여 각 면의 안쪽을 향하는 Normal(법선) 생성
        float3 planeNormals[4];
        planeNormals[0] = normalize(cross(vTL_Near - vBL_Near, vBL_Far - vBL_Near)); 
        planeNormals[1] = normalize(cross(vTR_Near - vTL_Near, vTL_Far - vTL_Near)); 
        planeNormals[2] = normalize(cross(vBR_Near - vTR_Near, vTR_Far - vTR_Near)); 
        planeNormals[3] = normalize(cross(vBL_Near - vBR_Near, vBR_Far - vBR_Near)); 
        // 평면 데이터 저장 (법선 및 원점과의 거리 d)
        for (int p = 0; p < 4; p++)
        {
            g_FrustumPlanes[p].Normal = planeNormals[p];
        }
        g_FrustumPlanes[0].DistanceToOrigin = -dot(planeNormals[0], vBL_Near);
        g_FrustumPlanes[1].DistanceToOrigin = -dot(planeNormals[1], vTL_Near);
        g_FrustumPlanes[2].DistanceToOrigin = -dot(planeNormals[2], vTR_Near);
        g_FrustumPlanes[3].DistanceToOrigin = -dot(planeNormals[3], vBR_Near);
    }
    GroupMemoryBarrierWithGroupSync();
}

[numthreads(16, 16, 1)]
void CS_Point(uint3 groupId : SV_GroupID, uint3 groupThreadId : SV_GroupThreadID, uint3 dispatchThreadId : SV_DispatchThreadID, uint groupIndex : SV_GroupIndex)
{
    uint screenWidth = 0, screenHeight = 0;
    DepthTexture.GetDimensions(screenWidth, screenHeight);

    InitializeTileAndFrustum(groupId, dispatchThreadId, groupIndex, screenWidth, screenHeight);

    float minDepthF = max(NearPlane, asfloat(g_MinDepthInt));
    float maxDepthF = min(FarPlane, asfloat(g_MaxDepthInt));
    uint tileStartSlice = GetZSlice(minDepthF);
    uint tileEndSlice = GetZSlice(maxDepthF);
    
	// Cluster내부에 빛의 영향이 있는 것 걸러내는 작업.
    for (uint i = groupIndex; i < PointLightCount; i += 256)
    {
        FPointLightInfo light = PointLightData[i];
        float3 LightViewPosition = mul(float4(light.Position.xyz, 1.0f), View).xyz;

        // Z-bounds검사
        if (LightViewPosition.z - light.AttenuationRadius > maxDepthF || LightViewPosition.z + light.AttenuationRadius < minDepthF)
            continue;

        bool bInFrustum = true;
        for (int p = 0; p < 4; p++)
        {
            if (dot(g_FrustumPlanes[p].Normal, LightViewPosition) + g_FrustumPlanes[p].DistanceToOrigin < -light.AttenuationRadius)
            {
                bInFrustum = false;
                break;
            }
        }

        if (bInFrustum)
        {
			// 빛이 겹치는 Cluster 층을 구하는중
            float lightMinZ = max(NearPlane, LightViewPosition.z - light.AttenuationRadius);
            float lightMaxZ = min(FarPlane, LightViewPosition.z + light.AttenuationRadius);

            uint lightStartSlice = GetZSlice(lightMinZ);
            uint lightEndSlice = GetZSlice(lightMaxZ);

            uint actualStart = max(tileStartSlice, lightStartSlice);
            uint actualEnd = min(tileEndSlice, lightEndSlice);

            // 영향을 받는 cluster 층에 빛 인덱스 추가
            for (uint z = actualStart; z <= actualEnd; z++)
            {
                uint slot;
                InterlockedAdd(g_ClusterLightCount[z], 1, slot);
				if (slot < MAX_LIGHTS_PER_CLUSTER)
				{
                    g_ClusterLightIndices[z][slot] = i;
                }
            }

        }
    }
    GroupMemoryBarrierWithGroupSync();

    uint numTilesX = (screenWidth + TILE_SIZE - 1) / TILE_SIZE;
    uint tileIndex = groupId.y * numTilesX + groupId.x;

    // 0 ~ 23 스레드만 동작.
    if (groupIndex < CLUSTER_SLICES)
    {
        uint z = groupIndex;
        uint count = min(g_ClusterLightCount[z], (uint) MAX_LIGHTS_PER_CLUSTER);

        uint cluster3DIndex = tileIndex * CLUSTER_SLICES + z;
        if (z >= tileStartSlice && z <= tileEndSlice && count > 0)
        {
			// GlobalCounts가 500000이 넘는 것에 대한 방어로직
            uint startOffset;
            InterlockedAdd(GlobalCounts[0], count, startOffset);

            if (startOffset >= MAX_GLOBAL_LIGHT_INDICES)
            {
                count = 0;
                startOffset = 0;
            }
			else if (startOffset + count > MAX_GLOBAL_LIGHT_INDICES)
			{
                count = MAX_GLOBAL_LIGHT_INDICES - startOffset;
            }
			// 해당하는 클러스터 인덱스에 정보 저장
            ClusterGrid[cluster3DIndex] = uint2(startOffset, count);
            g_ClusterStartOffset[z] = startOffset;
            g_ClusterLightCount[z] = count;
        }
		else
		{
            ClusterGrid[cluster3DIndex] = uint2(0, 0);
            g_ClusterLightCount[z] = 0;
        }
    }
    GroupMemoryBarrierWithGroupSync();

    for (uint idx = groupIndex; idx < CLUSTER_SLICES * MAX_LIGHTS_PER_CLUSTER; idx += 256)
    {
        // 1차원 배열 2차원 배열화
        uint z = idx / MAX_LIGHTS_PER_CLUSTER;
        uint lightSlot = idx % MAX_LIGHTS_PER_CLUSTER;

        if (lightSlot < g_ClusterLightCount[z])
        {
			// groupshared에는 배열화 되어 있는데 이것을 1차원 GlobalIndices에 저장하는 과정임.
            uint offset = g_ClusterStartOffset[z];
            uint lightIndex = g_ClusterLightIndices[z][lightSlot];
            GlobalIndices[offset + lightSlot] = lightIndex;
        }
    }

}


[numthreads(16, 16, 1)]
void CS_Spot(uint3 groupId : SV_GroupID, uint3 groupThreadId : SV_GroupThreadID, uint3 dispatchThreadId : SV_DispatchThreadID, uint groupIndex : SV_GroupIndex)
{
    uint screenWidth = 0, screenHeight = 0;
    DepthTexture.GetDimensions(screenWidth, screenHeight);

    InitializeTileAndFrustum(groupId, dispatchThreadId, groupIndex, screenWidth, screenHeight);

    float minDepthF = max(NearPlane, asfloat(g_MinDepthInt));
    float maxDepthF = min(FarPlane, asfloat(g_MaxDepthInt));
    uint tileStartSlice = GetZSlice(minDepthF);
    uint tileEndSlice = GetZSlice(maxDepthF);
    
	// Cluster내부에 빛의 영향이 있는 것 걸러내는 작업.
    for (uint i = groupIndex; i < SpotLightCount; i += 256)
    {
        FSpotLightInfo light = SpotLightData[i];
        float3 apexViewPos = mul(float4(light.Position.xyz, 1.0f), View).xyz;
        float3 viewDir = normalize(mul(float4(light.Direction.xyz, 0.0f), View).xyz);
        
        float coneLength = light.AttenuationRadius;
        float halfAngle = light.OuterConeAngle;

        float3 boundingCenter = apexViewPos;
        float boundingRadius = coneLength;

        if (halfAngle <= 0.785398f)
        {
            boundingRadius = coneLength / (2.0f * cos(halfAngle));
            boundingCenter = apexViewPos + (viewDir * boundingRadius);
        }

        if (boundingCenter.z - boundingRadius > maxDepthF || boundingCenter.z + boundingRadius < minDepthF)
            continue;

        bool bInFrustum = true;
        for (int p = 0; p < 4; p++)
        {
            if (dot(g_FrustumPlanes[p].Normal, boundingCenter) + g_FrustumPlanes[p].DistanceToOrigin < -boundingRadius)
            {
                bInFrustum = false;
                break;
            }
        }

        if (bInFrustum)
        {
            // SpotLight의 Z-Bound 교집합 계산
            float lightMinZ = max(NearPlane, boundingCenter.z - boundingRadius);
            float lightMaxZ = min(FarPlane, boundingCenter.z + boundingRadius);

            uint lightStartSlice = GetZSlice(lightMinZ);
            uint lightEndSlice = GetZSlice(lightMaxZ);

            uint actualStart = max(tileStartSlice, lightStartSlice);
            uint actualEnd = min(tileEndSlice, lightEndSlice);

            for (uint z = actualStart; z <= actualEnd; ++z)
            {
                uint slot;
                InterlockedAdd(g_ClusterLightCount[z], 1, slot);
                if (slot < MAX_LIGHTS_PER_CLUSTER)
                {
                    g_ClusterLightIndices[z][slot] = i;
                }
            }
        }
    }
    GroupMemoryBarrierWithGroupSync();

    uint numTilesX = (screenWidth + TILE_SIZE - 1) / TILE_SIZE;
    uint tileIndex = groupId.y * numTilesX + groupId.x;

    // 0 ~ 23 스레드만 동작.
    if (groupIndex < CLUSTER_SLICES)
    {
        uint z = groupIndex;
        uint count = min(g_ClusterLightCount[z], (uint) MAX_LIGHTS_PER_CLUSTER);

        uint cluster3DIndex = tileIndex * CLUSTER_SLICES + z;
        if (z >= tileStartSlice && z <= tileEndSlice && count > 0)
        {
			// GlobalCounts가 500000이 넘는 것에 대한 방어로직
            uint startOffset;
            InterlockedAdd(GlobalCounts[0], count, startOffset);

            if (startOffset >= MAX_GLOBAL_LIGHT_INDICES)
            {
                count = 0;
                startOffset = 0;
            }
            else if (startOffset + count > MAX_GLOBAL_LIGHT_INDICES)
            {
                count = MAX_GLOBAL_LIGHT_INDICES - startOffset;
            }
			// 해당하는 클러스터 인덱스에 정보 저장
            ClusterGrid[cluster3DIndex] = uint2(startOffset, count);
            g_ClusterStartOffset[z] = startOffset;
            g_ClusterLightCount[z] = count;
        }
        else
        {
            ClusterGrid[cluster3DIndex] = uint2(0, 0);
            g_ClusterLightCount[z] = 0;
        }
    }
    GroupMemoryBarrierWithGroupSync();

    for (uint idx = groupIndex; idx < CLUSTER_SLICES * MAX_LIGHTS_PER_CLUSTER; idx += 256)
    {
        uint z = idx / MAX_LIGHTS_PER_CLUSTER;
        uint lightSlot = idx % MAX_LIGHTS_PER_CLUSTER;

        if (lightSlot < g_ClusterLightCount[z])
        {
            uint offset = g_ClusterStartOffset[z];
            uint lightIndex = g_ClusterLightIndices[z][lightSlot];
            GlobalIndices[offset + lightSlot] = lightIndex;
        }
    }
}
#include "Common/Functions.hlsl"
#include "Common/VertexLayouts.hlsl"
#include "Common/ConstantBuffers.hlsl"

// SRV
//StructuredBuffer<FLightData> g_Lights : register(t0);
Texture2D<float> DepthTexture : register(t1);

// UAV
RWStructuredBuffer<uint> LightIndices : register(u0);
RWStructuredBuffer<uint> LightCounts : register(u1);

#define TILE_SIZE 16
#define MAX_LIGHTS_PER_TILE 256

groupshared uint g_MinDepthInt;
groupshared uint g_MaxDepthInt;
groupshared uint g_TileLightCount;
groupshared uint g_TileLightIndices[MAX_LIGHTS_PER_TILE];

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

void InitializeTileAndFrustum(uint3 groupId, uint3 dispatchThreadId, uint groupIndex, uint screenWidth, uint screenHeight)
{
    if (groupIndex == 0)
    {
        g_MinDepthInt = 0x7f7fffff;
        g_MaxDepthInt = 0;
        g_TileLightCount = 0;
        
        for (int i = 0; i < MAX_LIGHTS_PER_TILE; ++i)
            g_TileLightIndices[i] = 0;
    }
    GroupMemoryBarrierWithGroupSync();

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
void CS_LocalLight(uint3 groupId : SV_GroupID, uint3 groupThreadId : SV_GroupThreadID, uint3 dispatchThreadId : SV_DispatchThreadID, uint groupIndex : SV_GroupIndex)
{
    uint screenWidth = 0, screenHeight = 0;
    DepthTexture.GetDimensions(screenWidth, screenHeight);

    InitializeTileAndFrustum(groupId, dispatchThreadId, groupIndex, screenWidth, screenHeight);

    float minDepthF = asfloat(g_MinDepthInt);
    float maxDepthF = asfloat(g_MaxDepthInt);

    // PointLightCount, PointLightData 사용
    for (uint i = groupIndex; i < LocalLightCount; i += 256)
    {
        FLightData light = LocalLightData[i];
        float3 viewPos = mul(float4(light.Position, 1.0f), View).xyz;

        float3 boundingCenter = viewPos;
        float boundingRadius = light.AttenuationRadius;

        // SpotLight의 경우 재계산
        if (light.LightType == 1)
        {
            float3 viewDir = normalize(mul(float4(light.Direction, 0.0f), View).xyz);
            if (light.OuterConeCos >= 0.707106f)
            {
                boundingRadius = light.AttenuationRadius / (2.0f * light.OuterConeCos);
                boundingCenter = viewPos + (viewDir * boundingRadius);
            }
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
            uint slot;
            InterlockedAdd(g_TileLightCount, 1, slot);
            if (slot < MAX_LIGHTS_PER_TILE)
                g_TileLightIndices[slot] = i;
        }
    }
    GroupMemoryBarrierWithGroupSync();

    uint numTilesX = (screenWidth + TILE_SIZE - 1) / TILE_SIZE;
    uint tileIndex = groupId.y * numTilesX + groupId.x;

    if (groupIndex == 0)
        LightCounts[tileIndex] = min(g_TileLightCount, (uint) MAX_LIGHTS_PER_TILE);
    
    uint exportCount = min(g_TileLightCount, (uint) MAX_LIGHTS_PER_TILE);
    for (uint j = groupIndex; j < exportCount; j += 256)
    {
        LightIndices[tileIndex * MAX_LIGHTS_PER_TILE + j] = g_TileLightIndices[j];
    }
}
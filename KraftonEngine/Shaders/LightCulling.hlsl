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
#define MAX_LIGHTS_PER_TILE 64

groupshared uint g_MinDepthInt;
groupshared uint g_MaxDepthInt;
groupshared uint g_TileLightCount;
groupshared uint g_TileLightIndices[MAX_LIGHTS_PER_TILE];

struct Plane
{
    float3 N;
    float d;
};
groupshared Plane g_FrustumPlanes[4];

float3 ScreenToView(float4 screenPos, float2 screenDims)
{
    float2 texCoord = screenPos.xy / screenDims;
    float4 clip = float4(texCoord.x * 2.0f - 1.0f, (1.0f - texCoord.y) * 2.0f - 1.0f, screenPos.z, screenPos.w);
    float4 view = mul(clip, InverseProjection);
    return view.xyz / view.w;
}

[numthreads(16, 16, 1)]
void CS(uint3 groupId : SV_GroupID,
		uint3 groupThreadId : SV_GroupThreadID,
		uint3 dispatchThreadId : SV_DispatchThreadID,
		uint groupIndex : SV_GroupIndex)
{
    uint screenWidth = 0, screenHeight = 0;
    DepthTexture.GetDimensions(screenWidth, screenHeight);

    // group값 초기화
    if (groupIndex == 0)
    {
        g_MinDepthInt = 0x7f7fffff;
        g_MaxDepthInt = 0;
        g_TileLightCount = 0;
    }

    // minZ, maxZ 계산
    GroupMemoryBarrierWithGroupSync();
    
    float viewZ = FarPlane;
    if (dispatchThreadId.x < screenWidth && dispatchThreadId.y < screenHeight)
    {
        float depth = DepthTexture.Load(int3(dispatchThreadId.xy, 0)).r;

        if (depth < 1.0f)
        {
            viewZ = 1.0f / (depth * InvDeviceZToWorldZTransform2 - InvDeviceZToWorldZTransform3);
        }
    }

    uint zInt = asuint(viewZ);
    InterlockedMin(g_MinDepthInt, zInt);
    InterlockedMax(g_MaxDepthInt, zInt);

    // 16x16픽셀의 절두체 평면 계산(각 그룹의 첫 번째 스레드가 담당)
    GroupMemoryBarrierWithGroupSync();

    if (groupIndex == 0)
    {
        float2 screenDims = float2((float) screenWidth, (float) screenHeight);

        uint2 tileMin = groupId.xy * TILE_SIZE;
        uint2 tileMax = tileMin + TILE_SIZE;

        float3 pBL = ScreenToView(float4(tileMin.x, tileMax.y, 1.0f, 1.0f), screenDims);
        float3 pTL = ScreenToView(float4(tileMin.x, tileMin.y, 1.0f, 1.0f), screenDims);
        float3 pTR = ScreenToView(float4(tileMax.x, tileMin.y, 1.0f, 1.0f), screenDims);
        float3 pBR = ScreenToView(float4(tileMax.x, tileMax.y, 1.0f, 1.0f), screenDims);

        float3 center = ScreenToView(float4((tileMin.x + tileMax.x) * 0.5f, (tileMin.y + tileMax.y) * 0.5f, 1.0f, 1.0f), screenDims);

        float3 planes[4];
        planes[0] = normalize(cross(pBL, pTL));
        planes[1] = normalize(cross(pTL, pTR));
        planes[2] = normalize(cross(pTR, pBR));
        planes[3] = normalize(cross(pBR, pBL));

        for (int p = 0; p < 4; p++)
        {
            if (dot(planes[p], center) < 0.0f)
            {
                planes[p] = -planes[p];
            }
            g_FrustumPlanes[p].N = planes[p];
            g_FrustumPlanes[p].d = 0.0f;
        }
    }
    // Light와 절두체의 Intersection 테스트 - 여기에서 충돌되면 해당 LightIndices 슬롯에 추가.
    GroupMemoryBarrierWithGroupSync();

    float minDepthF = asfloat(g_MinDepthInt);
    float maxDepthF = asfloat(g_MaxDepthInt);

    /*for (uint i = groupIndex; i < g_ActiveLightCount; i += 256)
    {
        FLightData light = g_Lights[i];

        float3 viewPos = mul(float4(light.Position, 1.0f), View).xyz;
        if (viewPos.z - light.Range > maxDepthF || viewPos.z + light.Range < minDepthF)
        {
            continue;
        }

        bool bInFrustum = true;
        for (int p = 0; p < 4; p++)
        {
            float dist = dot(g_FrustumPlanes[p].N, viewPos);
            if (dist < -light.Range)
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
            {
                g_TileLightIndices[slot] = i;
            }
        }
    }*/
    GroupMemoryBarrierWithGroupSync();

    uint numTilesX = (screenWidth + TILE_SIZE - 1) / TILE_SIZE;
    uint tileIndex = groupId.y * numTilesX + groupId.x;

    if (groupIndex == 0)
    {
        LightCounts[tileIndex] = min(g_TileLightCount, (uint) MAX_LIGHTS_PER_TILE);
    }

    uint exportCount = min(g_TileLightCount, (uint) MAX_LIGHTS_PER_TILE);
    for (uint i = groupIndex; i < exportCount; i += 256)
    {
        LightIndices[tileIndex * MAX_LIGHTS_PER_TILE + i] = g_TileLightIndices[i];
    }

}
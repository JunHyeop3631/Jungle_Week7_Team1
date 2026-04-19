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
            viewZ = 1.0f / (depth * InvDeviceZToWorldZTransform2 - InvDeviceZToWorldZTransform3);
        }
    }

    uint zInt = asuint(viewZ);
    InterlockedMin(g_MinDepthInt, zInt);
    InterlockedMax(g_MaxDepthInt, zInt);
    GroupMemoryBarrierWithGroupSync();

    if (groupIndex == 0)
    {
        float2 screenDims = float2((float) screenWidth, (float) screenHeight);
        uint2 tileMin = groupId.xy * TILE_SIZE;
        uint2 tileMax = tileMin + TILE_SIZE;

        float3 viewBottomLeft = ScreenToView(float4(tileMin.x, tileMax.y, 1.0f, 1.0f), screenDims);
        float3 viewTopLeft = ScreenToView(float4(tileMin.x, tileMin.y, 1.0f, 1.0f), screenDims);
        float3 viewTopRight = ScreenToView(float4(tileMax.x, tileMin.y, 1.0f, 1.0f), screenDims);
        float3 viewBottomRight = ScreenToView(float4(tileMax.x, tileMax.y, 1.0f, 1.0f), screenDims);
        
        float3 viewCenter = ScreenToView(float4((tileMin.x + tileMax.x) * 0.5f, (tileMin.y + tileMax.y) * 0.5f, 1.0f, 1.0f), screenDims);

        float3 planeNormals[4];
        planeNormals[0] = normalize(cross(viewBottomLeft, viewTopLeft));
        planeNormals[1] = normalize(cross(viewTopLeft, viewTopRight));
        planeNormals[2] = normalize(cross(viewTopRight, viewBottomRight));
        planeNormals[3] = normalize(cross(viewBottomRight, viewBottomLeft));

        for (int p = 0; p < 4; p++)
        {
            if (dot(planeNormals[p], viewCenter) < 0.0f)
            {
                planeNormals[p] = -planeNormals[p];
            }
                
            g_FrustumPlanes[p].Normal = planeNormals[p];
            g_FrustumPlanes[p].DistanceToOrigin = 0.0f;
        }
    }
    GroupMemoryBarrierWithGroupSync();
}

[numthreads(16, 16, 1)]
void CS_Point(uint3 groupId : SV_GroupID, uint3 groupThreadId : SV_GroupThreadID, uint3 dispatchThreadId : SV_DispatchThreadID, uint groupIndex : SV_GroupIndex)
{
    uint screenWidth = 0, screenHeight = 0;
    DepthTexture.GetDimensions(screenWidth, screenHeight);

    InitializeTileAndFrustum(groupId, dispatchThreadId, groupIndex, screenWidth, screenHeight);

    float minDepthF = asfloat(g_MinDepthInt);
    float maxDepthF = asfloat(g_MaxDepthInt);

    // PointLightCount, PointLightData 사용
    for (uint i = groupIndex; i < PointLightCount; i += 256)
    {
        FPointLightInfo light = PointLightData[i];
        float3 LightViewPosition = mul(float4(light.Position.xyz, 1.0f), View).xyz;

        if (LightViewPosition.z - light.AttenuationRadius > maxDepthF || LightViewPosition.z + light.AttenuationRadius < minDepthF)
            continue;

        bool bInFrustum = true;
        for (int p = 0; p < 4; p++)
        {
            if (dot(g_FrustumPlanes[p].Normal, LightViewPosition) < -light.AttenuationRadius)
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


// 로직 자체는 PointLight와 동일 -> 원뿔 계산이 비효율적이므로 동일하게 사용 -> 자세하게 한다면 로직 변경 필요
[numthreads(16, 16, 1)]
void CS_Spot(uint3 groupId : SV_GroupID, uint3 groupThreadId : SV_GroupThreadID, uint3 dispatchThreadId : SV_DispatchThreadID, uint groupIndex : SV_GroupIndex)
{
    uint screenWidth = 0, screenHeight = 0;
    DepthTexture.GetDimensions(screenWidth, screenHeight);

    InitializeTileAndFrustum(groupId, dispatchThreadId, groupIndex, screenWidth, screenHeight);

    float minDepthF = asfloat(g_MinDepthInt);
    float maxDepthF = asfloat(g_MaxDepthInt);
    
    for (uint i = groupIndex; i < SpotLightCount; i += 256)
    {
        FSpotLightInfo light = SpotLightData[i];

        float3 apexViewPos = mul(float4(light.Position.xyz, 1.0f), View).xyz; // SpotLight의 꼭짓점
        float3 viewDir = mul(float4(light.Direction.xyz, 0.0f), View).xyz;
        viewDir = normalize(viewDir);
        
        float coneLength = light.AttenuationRadius;
        float halfAngle = light.OuterConeAngle;

        float3 boundingCenter = apexViewPos;
        float boundingRadius = coneLength;

        // 45도 이하인 경우에만 원뿔의 중심, 반지름 재계산.
        if (halfAngle <= 0.785398f) // 45도 이하에만 적용
        {
            boundingRadius = coneLength / (2.0f * cos(halfAngle));
            boundingCenter = apexViewPos + (viewDir * boundingRadius);
        }


        // 구와 절두체 컬링(기존과 동일)
        if (boundingCenter.z - boundingRadius > maxDepthF || boundingCenter.z + boundingRadius < minDepthF)
            continue;

        bool bInFrustum = true;
        for (int p = 0; p < 4; p++)
        {
            if (dot(g_FrustumPlanes[p].Normal, boundingCenter) < -boundingRadius)
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
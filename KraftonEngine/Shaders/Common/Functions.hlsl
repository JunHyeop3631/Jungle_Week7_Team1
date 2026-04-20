#ifndef FUNCTIONS_HLSL
#define FUNCTIONS_HLSL

#include "Common/ConstantBuffers.hlsl"
#include "Common/VertexLayouts.hlsl"

// Model -> View -> Projection 변환
float4 ApplyMVP(float3 pos)
{
    float4 world = mul(float4(pos, 1.0f), Model);
    float4 view = mul(world, View);
    return mul(view, Projection);
}

// View -> Projection 변환 (CPU 빌보드용 — 이미 월드 좌표인 정점)
float4 ApplyVP(float3 worldPos)
{
    return mul(mul(float4(worldPos, 1.0f), View), Projection);
}

// 와이어프레임 모드 적용 — baseColor를 WireframeRGB로 대체
float3 ApplyWireframe(float3 baseColor)
{
    return lerp(baseColor, WireframeRGB, bIsWireframe);
}

// 폰트 아틀라스 배경 디스카드 판정
bool ShouldDiscardFontPixel(float sampledRed)
{
    return sampledRed < 0.1f;
}

float3 GetWorldNormal(PS_Lighting input, Texture2D normalMap, SamplerState sam)
{
    float3 mapNormal = normalMap.Sample(sam, input.texCoord).rgb;
    mapNormal = mapNormal * 2.0f - 1.0f;

    float3 N = normalize(input.worldNormal);
    float3 T = normalize(input.worldTangent.xyz - N * dot(N, input.worldTangent.xyz));
    float3 B = normalize(cross(N, T)) * input.worldTangent.w;
    
    float3x3 TBN = float3x3(T, B, N);
    return mul(mapNormal, TBN);
}

struct LightingResult
{
    float3 Diffuse;
    float3 Specular;
};

// ============================================================
// Directional Light (전역 조명이므로 컬링 불필요)
// ============================================================
LightingResult ComputeDirectionalLight_BlinnPhong(float3 cameraPos, float3 worldPos, float3 worldNormal, float exp)
{
    LightingResult result = (LightingResult) 0;
    float3 diffuse = 0;
    float3 specular = 0;
    
    float3 N = normalize(worldNormal);
    float3 V = normalize(cameraPos - worldPos);
    float3 L = normalize(-Directional.Direction.xyz);
    
    float NdotLRaw = dot(N, L);
    float NdotL = max(NdotLRaw, 0.0f);
    
    diffuse += Directional.LightColor.rgb * NdotL;
    
    if (NdotLRaw > 0.0f)
    {
        float3 halfDir = normalize(L + V);
        float specIntensity = pow(max(dot(N, halfDir), 0.0f), exp);
        specular = Directional.LightColor.rgb * specIntensity;
    }
    
    result.Diffuse = diffuse;
    result.Specular = specular;
    return result;
}

LightingResult ComputeDirectionalLight_Lambert(float3 worldNormal)
{
    LightingResult result = (LightingResult) 0;
    float3 diffuse = 0.0f;
    float3 N = normalize(worldNormal);
    float3 L = normalize(-Directional.Direction.xyz);
    
    float diffIntensity = saturate(dot(N, L) * 0.5f + 0.5f);
    diffuse += Directional.LightColor.rgb * diffIntensity;
        
    result.Diffuse = diffuse;
    return result;
}

// ============================================================
// [VS용] 타일 컬링을 사용하지 않는 순회 함수들 (_NoTile)
// ============================================================
LightingResult ComputePointLight_BlinnPhong_NoTile(float3 cameraPos, float3 worldPos, float3 worldNormal, float shininess)
{
    LightingResult result = (LightingResult) 0;
    float3 diffuse = 0.0f, specular = 0.0f;
    float3 N = normalize(worldNormal);
    float3 V = normalize(cameraPos - worldPos);

    for (uint i = 0; i < PointLightCount; ++i)
    {
        FPointLightInfo light = PointLightData[i];
        float3 toLight = light.Position.xyz - worldPos;
        float dist = length(toLight);
        float3 L = toLight / max(dist, 0.0001f);
        
        float NdotLRaw = dot(N, L);
        float NdotL = max(NdotLRaw, 0.0f);

        float distanceAtten = saturate(1.0f - dist / light.AttenuationRadius);
        float atten = pow(distanceAtten, light.FalloffExponent);

        diffuse += light.LightColor.rgb * NdotL * atten;

        if (NdotLRaw > 0.0f && atten > 0.0f)
        {
            float3 H = normalize(L + V);
            float spec = pow(max(dot(N, H), 0.0f), shininess);
            specular += light.LightColor.rgb * spec * atten;
        }
    }
    result.Diffuse = diffuse;
    result.Specular = specular;
    return result;
}

LightingResult ComputeSpotLight_BlinnPhong_NoTile(float3 cameraPos, float3 worldPos, float3 worldNormal, float shininess)
{
    LightingResult result = (LightingResult) 0;
    float3 diffuse = 0.0f, specular = 0.0f;
    float3 N = normalize(worldNormal);
    float3 V = normalize(cameraPos - worldPos);

    for (uint i = 0; i < SpotLightCount; ++i)
    {
        FSpotLightInfo light = SpotLightData[i];
        float3 toLight = light.Position.xyz - worldPos;
        float dist = length(toLight);
        float3 L = toLight / max(dist, 0.0001f);
        
        float3 lightDir = normalize(light.Direction.xyz);
        float distanceAtten = saturate(1.0f - dist / light.AttenuationRadius);
        float spotCos = dot(lightDir, -L);
        float spotFactor = saturate((spotCos - light.OuterConeAngle) / max(light.InnerConeAngle - light.OuterConeAngle, 0.0001f));
        float atten = distanceAtten * spotFactor;
        
        float NdotLRaw = dot(N, L);
        float NdotL = max(NdotLRaw, 0.0f);

        diffuse += light.LightColor.rgb * NdotL * atten;

        if (NdotLRaw > 0.0f && atten > 0.0f)
        {
            float3 H = normalize(L + V);
            float spec = pow(max(dot(N, H), 0.0f), shininess);
            specular += light.LightColor.rgb * spec * atten;
        }
    }
    result.Diffuse = diffuse;
    result.Specular = specular;
    return result;
}

LightingResult ComputePointLight_Lambert_NoTile(float3 worldPos, float3 worldNormal)
{
    LightingResult result = (LightingResult) 0;
    float3 diffuse = 0.0f;
    float3 N = normalize(worldNormal);
    for (uint i = 0; i < PointLightCount; ++i)
    {
        FPointLightInfo light = PointLightData[i];
        float3 toLight = light.Position.xyz - worldPos;
        float dist = length(toLight);
        float3 L = toLight / max(dist, 0.0001f);
        float NdotL = saturate(dot(N, L) * 0.5f + 0.5f);
        float distanceAtten = saturate(1.0f - dist / light.AttenuationRadius);
        float atten = pow(distanceAtten, light.FalloffExponent);
        
        diffuse += light.LightColor.rgb * NdotL * atten;
    }
    result.Diffuse = diffuse;
    return result;
}

LightingResult ComputeSpotLight_Lambert_NoTile(float3 worldPos, float3 worldNormal)
{
    LightingResult result = (LightingResult) 0;
    float3 diffuse = 0.0f;
    float3 N = normalize(worldNormal);
    for (uint i = 0; i < SpotLightCount; ++i)
    {
        FSpotLightInfo light = SpotLightData[i];
        float3 toLight = light.Position.xyz - worldPos;
        float dist = length(toLight);
        float3 L = toLight / max(dist, 0.0001f);
        float NdotL = saturate(dot(N, L) * 0.5f + 0.5f);
        float3 lightDir = normalize(light.Direction.xyz);
        float distanceAtten = saturate(1.0f - dist / light.AttenuationRadius);
        float spotCos = dot(lightDir, -L);
        float spotFactor = saturate((spotCos - light.OuterConeAngle) / max(light.InnerConeAngle - light.OuterConeAngle, 0.0001f));
        
        diffuse += light.LightColor.rgb * NdotL * distanceAtten * spotFactor;
    }
    result.Diffuse = diffuse;
    return result;
}


// ============================================================
// [PS용] 타일 컬링이 적용된 핵심 조명 계산 함수들 (Tile-Culled)
// ============================================================
LightingResult ComputePointLight_BlinnPhong(float3 cameraPos, float3 worldPos, float3 worldNormal, float shininess, float2 screenPos)
{
    LightingResult result = (LightingResult) 0;
    float3 diffuse = 0.0f;
    float3 specular = 0.0f;

    float3 N = normalize(worldNormal);
    float3 V = normalize(cameraPos - worldPos);

    uint tileX = (uint) screenPos.x / 16;
    uint tileY = (uint) screenPos.y / 16;
    uint numTilesX = ((uint) ScreenWidth + 15) / 16;
    uint tileIndex = tileY * numTilesX + tileX;

    uint lightCount = PointLightCounts[tileIndex];
    uint tileOffset = tileIndex * 64;
    
    for (uint i = 0; i < lightCount; ++i)
    {
        uint lightIndex = PointLightIndices[tileOffset + i];
        FPointLightInfo light = PointLightData[lightIndex];
        
        float3 toLight = light.Position.xyz - worldPos;
        float dist = length(toLight);
        float3 L = toLight / max(dist, 0.0001f);

        float distanceAtten = saturate(1.0f - dist / light.AttenuationRadius);
        float atten = pow(distanceAtten, light.FalloffExponent);

        float NdotLRaw = dot(N, L);
        float NdotL = max(NdotLRaw, 0.0f);

        diffuse += light.LightColor.rgb * NdotL * atten;

        if (NdotLRaw > 0.0f && atten > 0.0f)
        {
            float3 H = normalize(L + V);
            float spec = pow(max(dot(N, H), 0.0f), shininess);
            specular += light.LightColor.rgb * spec * atten;
        }
    }

    result.Diffuse = diffuse;
    result.Specular = specular;
    return result;
}

LightingResult ComputeSpotLight_BlinnPhong(float3 cameraPos, float3 worldPos, float3 worldNormal, float shininess, float2 screenPos)
{
    LightingResult result = (LightingResult) 0;
    float3 diffuse = 0.0f;
    float3 specular = 0.0f;
    float3 N = normalize(worldNormal);
    float3 V = normalize(cameraPos - worldPos);

    uint tileX = (uint) screenPos.x / 16;
    uint tileY = (uint) screenPos.y / 16;
    uint numTilesX = ((uint) ScreenWidth + 15) / 16;
    uint tileIndex = tileY * numTilesX + tileX;
    
    uint lightCount = SpotLightCounts[tileIndex];
    uint tileOffset = tileIndex * 64; // MAX_LIGHTS_PER_TILE

    for (uint i = 0; i < lightCount; ++i)
    {
        uint lightIndex = SpotLightIndices[tileOffset + i];
        FSpotLightInfo light = SpotLightData[lightIndex];

        float3 toLight = light.Position.xyz - worldPos;
        float dist = length(toLight);
        float3 L = toLight / max(dist, 0.0001f);
        
        float3 lightDir = normalize(light.Direction.xyz);
        float distanceAtten = saturate(1.0f - dist / light.AttenuationRadius);
        float spotCos = dot(lightDir, -L);
        float spotFactor = saturate((spotCos - light.OuterConeAngle) / max(light.InnerConeAngle - light.OuterConeAngle, 0.0001f));
        float atten = distanceAtten * spotFactor;
        
        float NdotLRaw = dot(N, L);
        float NdotL = max(NdotLRaw, 0.0f);

        diffuse += light.LightColor.rgb * NdotL * atten;

        if (NdotLRaw > 0.0f && atten > 0.0f)
        {
            float3 H = normalize(L + V);
            float spec = pow(max(dot(N, H), 0.0f), shininess);
            specular += light.LightColor.rgb * spec * atten;
        }
    }

    result.Diffuse = diffuse;
    result.Specular = specular;
    return result;
}

LightingResult ComputePointLight_Lambert(float3 worldPos, float3 worldNormal, float2 screenPos)
{
    LightingResult result = (LightingResult) 0;
    float3 diffuse = 0.0f;
    float3 N = normalize(worldNormal);
    
    uint tileX = (uint) screenPos.x / 16;
    uint tileY = (uint) screenPos.y / 16;
    uint numTilesX = ((uint) ScreenWidth + 15) / 16;
    uint tileIndex = tileY * numTilesX + tileX;

    uint lightCount = PointLightCounts[tileIndex];
    uint tileOffset = tileIndex * 64;

    for (uint i = 0; i < lightCount; ++i)
    {
        uint lightIndex = PointLightIndices[tileOffset + i];
        FPointLightInfo light = PointLightData[lightIndex];
        
        float3 toLight = light.Position.xyz - worldPos;
        float dist = length(toLight);
        float3 L = toLight / max(dist, 0.0001f);
        float NdotL = saturate(dot(N, L) * 0.5f + 0.5f);
        float distanceAtten = saturate(1.0f - dist / light.AttenuationRadius);
        float atten = pow(distanceAtten, light.FalloffExponent);
        
        diffuse += light.LightColor.rgb * NdotL * atten;
    }
    result.Diffuse = diffuse;
    return result;
}

LightingResult ComputeSpotLight_Lambert(float3 worldPos, float3 worldNormal, float2 screenPos)
{
    LightingResult result = (LightingResult) 0;
    float3 diffuse = 0.0f;
    float3 N = normalize(worldNormal);
    
    uint tileX = (uint) screenPos.x / 16;
    uint tileY = (uint) screenPos.y / 16;
    uint numTilesX = ((uint) ScreenWidth + 15) / 16;
    uint tileIndex = tileY * numTilesX + tileX;

    uint lightCount = SpotLightCounts[tileIndex];
    uint tileOffset = tileIndex * 64;

    for (uint i = 0; i < lightCount; ++i)
    {
        uint lightIndex = SpotLightIndices[tileOffset + i];
        FSpotLightInfo light = SpotLightData[lightIndex];
        
        float3 toLight = light.Position.xyz - worldPos;
        float dist = length(toLight);
        float3 L = toLight / max(dist, 0.0001f);
        float NdotL = saturate(dot(N, L) * 0.5f + 0.5f);
        float3 lightDir = normalize(light.Direction.xyz);
        float distanceAtten = saturate(1.0f - dist / light.AttenuationRadius);
        float spotCos = dot(lightDir, -L);
        float spotFactor = saturate((spotCos - light.OuterConeAngle) / max(light.InnerConeAngle - light.OuterConeAngle, 0.0001f));
        
        diffuse += light.LightColor.rgb * NdotL * distanceAtten * spotFactor;
    }
    result.Diffuse = diffuse;
    return result;
}

#endif // FUNCTIONS_HLSL

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
    float3 T = normalize(input.worldTangent);
    float3 B = normalize(cross(N, T) * input.tangentSign);

    float3x3 TBN = float3x3(T, B, N);
    return normalize(mul(mapNormal, TBN));
}

struct LightingResult
{
    float3 Diffuse;
    float3 Specular;
};

//Lighting 관련 함수들
LightingResult ComputeDirectionalLight_BlinnPhong(float3 cameraPos, float3 worldPos, float3 worldNormal, float exp)
{
    LightingResult result = (LightingResult) 0;
    float3 diffuse = 0;
    float3 specular = 0;
    
    float3 N = normalize(worldNormal);
    float3 V = normalize(cameraPos - worldPos);
    
    float3 L = normalize(-Directional.Direction);
    
    float diffIntensity = max(dot(N, L), 0.0f);
    diffuse += Directional.LightColor * diffIntensity;
        
    if (diffIntensity > 0.0f)
    {
        float3 halfDir = normalize(L + V);
        float specIntensity = pow(max(dot(N, halfDir), 0.0f), exp);
        specular = Directional.LightColor * specIntensity;
    }
    
    result.Diffuse = diffuse;
    result.Specular = specular;
    return result;
}

LightingResult ComputePointLight_BlinnPhong(
    float3 cameraPos,
    float3 worldPos,
    float3 worldNormal,
    float shininess)
{
    LightingResult result = (LightingResult) 0;
    float3 diffuse = 0.0f;
    float3 specular = 0.0f;

    float3 N = normalize(worldNormal);
    float3 V = normalize(cameraPos - worldPos);

    for (uint i = 0; i < NUM_POINT_LIGHT; ++i)
    {
        float3 toLight = PointLights[i].Position - worldPos;
        float dist = length(toLight);
        float3 L = toLight / max(dist, 0.0001f);

        float NdotL = max(dot(N, L), 0.0f);
        float distanceAtten = saturate(1.0f - dist / PointLights[i].AttenuationRadius);
        distanceAtten = pow(distanceAtten, PointLights[i].FalloffExponent);
        
        float atten = distanceAtten;

        diffuse += PointLights[i].LightColor.rgb * NdotL * atten;

        if (NdotL > 0.0f && atten > 0.0f)
        {
            float3 H = normalize(L + V);
            float spec = pow(max(dot(N, H), 0.0f), shininess);
            specular += PointLights[i].LightColor.rgb * spec * atten;
        }
    }

    result.Diffuse = diffuse;
    result.Specular = specular;
    return result;  
}

LightingResult ComputeSpotLight_BlinnPhong(
    float3 cameraPos,
    float3 worldPos,
    float3 worldNormal,
    float shininess)
{
    LightingResult result = (LightingResult) 0; 
    float3 diffuse = 0.0f;
    float3 specular = 0.0f;

    float3 N = normalize(worldNormal);
    float3 V = normalize(cameraPos - worldPos);

    for (uint i = 0; i < NUM_SPOT_LIGHT; ++i)
    {
        float3 toLight = SpotLights[i].Position - worldPos;
        float dist = length(toLight);
        float3 L = toLight / max(dist, 0.0001f);

        float NdotL = max(dot(N, L), 0.0f);

        float3 lightDir = normalize(SpotLights[i].Direction);
        float distanceAtten = saturate(1.0f - dist / SpotLights[i].AttenuationRadius);
        float spotCos = dot(lightDir, -L);
        float spotFactor = saturate(
            (spotCos - SpotLights[i].OuterConeAngle) /
            max(SpotLights[i].InnerConeAngle - SpotLights[i].OuterConeAngle, 0.0001f)
        );

        float atten = distanceAtten * spotFactor;

        diffuse += SpotLights[i].LightColor.rgb * NdotL * atten;

        if (NdotL > 0.0f && atten > 0.0f)
        {
            float3 H = normalize(L + V);
            float spec = pow(max(dot(N, H), 0.0f), shininess);
            specular += SpotLights[i].LightColor.rgb * spec * atten;
        }
    }

    result.Diffuse = diffuse;
    result.Specular = specular;
    return result;  
}

LightingResult ComputeDirectLight_Lambert(float3 worldNormal)
{
    LightingResult result = (LightingResult) 0;
    float3 diffuse = 0.0f;
    float3 N = normalize(worldNormal);
    float3 L = normalize(-Directional.Direction);
    
    float diffIntensity = max(dot(N, L), 0.0f);
    diffuse += Directional.LightColor * diffIntensity;
        
    result.Diffuse = diffuse;
    return result;
}

LightingResult ComputePointLight_Lambert(float3 worldPos, float3 worldNormal)
{
    LightingResult result = (LightingResult) 0;
    float3 diffuse = 0.0f;
    float3 N = normalize(worldNormal);
    for (uint i = 0; i < NUM_POINT_LIGHT; ++i)
    {
        float3 toLight = PointLights[i].Position - worldPos;
        float dist = length(toLight);
        float3 L = toLight / max(dist, 0.0001f);
        float NdotL = max(dot(N, L), 0.0f);
        float distanceAtten = saturate(1.0f - dist / PointLights[i].AttenuationRadius);
        distanceAtten = pow(distanceAtten, PointLights[i].FalloffExponent);
        
        diffuse += PointLights[i].LightColor.rgb * NdotL * distanceAtten;
    }
    result.Diffuse = diffuse;
    return result;  
}

LightingResult ComputeSpotLight_Lambert(float3 worldPos, float3 worldNormal)
{
    LightingResult result = (LightingResult) 0; 
    float3 diffuse = 0.0f;
    float3 N = normalize(worldNormal);
    for (uint i = 0; i < NUM_SPOT_LIGHT; ++i)
    {
        float3 toLight = SpotLights[i].Position - worldPos;
        float dist = length(toLight);
        float3 L = toLight / max(dist, 0.0001f);
        float NdotL = max(dot(N, L), 0.0f);
        float3 lightDir = normalize(SpotLights[i].Direction);
        float distanceAtten = saturate(1.0f - dist / SpotLights[i].AttenuationRadius);
        float spotCos = dot(lightDir, -L);
        float spotFactor = saturate(
            (spotCos - SpotLights[i].OuterConeAngle) /
            max(SpotLights[i].InnerConeAngle - SpotLights[i].OuterConeAngle, 0.0001f)
        );
        diffuse += SpotLights[i].LightColor.rgb * NdotL * distanceAtten * spotFactor;
    }
    result.Diffuse = diffuse;
    return result;  
}

#endif // FUNCTIONS_HLSL

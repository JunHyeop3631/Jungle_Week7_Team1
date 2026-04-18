#ifndef FUNCTIONS_HLSL
#define FUNCTIONS_HLSL

#include "Common/ConstantBuffers.hlsl"

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

#endif // FUNCTIONS_HLSL
float3 CalculatePointLight(FLightData light, float3 normal, float3 worldPos, float3 baseColor)
{
    // 1. 빛의 방향과 거리(제곱) 구하기
    float3 lightDir = light.Position - worldPos;
    
    // 🚀 [최적화] length() 대신 dot()을 사용해 거리의 '제곱'을 바로 구합니다. (sqrt 절약)
    float distSqr = dot(lightDir, lightDir);
    float rangeSqr = light.Range * light.Range;
    
    // 거리가 범위 밖이면 계산할 필요 없이 0 (검은색) 반환
    if (distSqr > rangeSqr)
    {
        return float3(0, 0, 0);
    }
    
    // 방향 벡터 정규화 (여기서만 sqrt를 한 번 씁니다)
    float distance = sqrt(distSqr);
    lightDir = lightDir / distance;

    // 2. 거리 감쇄 (Attenuation) - Unreal Engine 4/5 PBR 스타일 🚀
    // a. 물리 기반 역제곱 법칙 (Inverse Square Law)
    // 거리가 0일 때 빛이 무한대로 폭발하는 것을 막기 위해 최소값(0.0001f)을 둡니다.
    float distanceAttenuation = 1.0f / max(distSqr, 0.0001f);
    
    // b. Falloff (Windowing) 함수: Light Range의 끝부분에서 빛을 부드럽게 0으로 소멸시킵니다.
    float distRatioSqr = distSqr / rangeSqr;
    float falloff = saturate(1.0f - (distRatioSqr * distRatioSqr));
    
    // 최종 감쇄율 = 역제곱 * (Falloff의 제곱)
    float attenuation = distanceAttenuation * (falloff * falloff);

    // 3. 난반사 (Diffuse - Lambertian)
    // 빛을 정면으로 받을수록(dot값이 1에 가까울수록) 밝아짐
    float nDotL = saturate(dot(normal, lightDir));
    
    // 4. 최종 계산: 본래 색상 * 빛 색상 * 밝기 * 각도 * 감쇄
    float3 diffuse = baseColor * light.Color * light.Intensity * nDotL * attenuation;

    return diffuse;
}
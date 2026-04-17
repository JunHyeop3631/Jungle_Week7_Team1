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
    // 1. 빛의 방향과 거리 구하기
    float3 lightDir = light.Position - worldPos;
    float distance = length(lightDir);
    
    // 거리가 범위 밖이면 계산할 필요 없이 0 (검은색) 반환
    if (distance > light.Range)
    {
        return float3(0, 0, 0);
    }
    
    // 방향 벡터 정규화
    lightDir = normalize(lightDir);

    // 2. 거리 감쇄 (Attenuation) - 언리얼 엔진 스타일의 부드러운 감쇄
    float attenuation = smoothstep(1.0f, 0.0f, distance / light.Range);

    // 3. 난반사 (Diffuse - Lambertian)
    // 빛을 정면으로 받을수록(dot값이 1에 가까울수록) 밝아짐
    float nDotL = saturate(dot(normal, lightDir));
    
    // 4. 최종 계산: 본래 색상 * 빛 색상 * 밝기 * 각도 * 감쇄
    float3 diffuse = baseColor * light.Color * light.Intensity * nDotL * attenuation;

    return diffuse;
}
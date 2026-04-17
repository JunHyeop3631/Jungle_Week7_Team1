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

float3 GetWorldNormal(PS_Input_Full input, Texture2D normalMap, SamplerState sam)
{
    // 1. 노멀맵 샘플링 (0~1 범위를 -1~1 범위로 변환)
    float3 mapNormal = normalMap.Sample(sam, input.texcoord).rgb;
    mapNormal = mapNormal * 2.0f - 1.0f;

    // 2. TBN 기저 벡터 구성
    float3 N = normalize(input.normal);
    float3 T = normalize(input.tangent.xyz);
    
    // 3. BiNormal 직접 계산 (Handedness 처리)
    // CPU에서 계산해 넘겨준 tangent.w (-1 또는 1)를 곱해 뒤집힘 보정
    float3 B = cross(N, T) * input.tangent.w;
    
    float3x3 TBN = float3x3(T, B, N);

    // 4. 탄젠트 공간 노멀을 월드 공간으로 변환
    return normalize(mul(mapNormal, TBN));
}

#endif // FUNCTIONS_HLSL

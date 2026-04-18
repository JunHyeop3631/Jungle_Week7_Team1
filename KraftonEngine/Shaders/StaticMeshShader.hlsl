#include "Common/Functions.hlsl"
#include "Common/VertexLayouts.hlsl"
#include "Common/ConstantBuffers.hlsl"

Texture2D g_txColor : register(t0);
SamplerState g_Sample : register(s0);
StructuredBuffer<FLightData> g_Lights : register(t1);

StructuredBuffer<uint> TileLightIndices : register(t2);
StructuredBuffer<uint> TileLightCounts : register(t3);

#define TILE_SIZE 16
#define MAX_LIGHTS_PER_TILE 64

PS_Input_Full VS(VS_Input_PNCT input)
{
    PS_Input_Full output;
    output.position = ApplyMVP(input.position);
    output.normal = normalize(mul(input.normal, (float3x3) Model));
    output.color = input.color * SectionColor;
    output.worldPos = mul(float4(input.position, 1.0f), Model).xyz;

    float2 texcoord = input.texcoord;
    if (bIsUVScroll != 0)
    {
        texcoord.x += Time * 0.5f; // 가로 방향으로 스크롤 예시
    }
    output.texcoord = texcoord;

    return output;
}

float4 PS(PS_Input_Full input) : SV_TARGET
{
    float4 texColor = g_txColor.Sample(g_Sample, input.texcoord);

    // Unbound SRV는 (0,0,0,0)을 반환 — 텍스처 미바인딩 시 white로 대체
    if (texColor.a < 0.001f)
    {
        //알파값이 0에 가까운 정상 텍스처, 투명 텍스처에서 문제 발생 가능
        texColor = float4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    // 1. 픽셀의 기본 텍스처 색상 (Base Color)
    float4 baseColor = texColor * input.color;
    
    float3 litColor = float3(0, 0, 0);
    float3 N = normalize(input.normal);

    uint2 pixelPos = uint2(input.position.xy);
    uint2 tilePos = pixelPos / TILE_SIZE;
    uint tileIndex = tilePos.y * g_NumTilesX + tilePos.x;

    uint lightCountInTile = TileLightCounts[tileIndex];

    // C++에서 넘겨준 조명 개수만큼 루프를 돕니다
    for (uint i = 0; i < lightCountInTile; ++i)
    {
        uint actualLightIndex = TileLightIndices[tileIndex * MAX_LIGHTS_PER_TILE + i];
        litColor += CalculatePointLight(g_Lights[actualLightIndex], N, input.worldPos, baseColor.rgb);
    }

    // 최소한의 밝기를 보장하는 환경광(Ambient) 추가 (10% 정도)
    litColor += baseColor.rgb * 0.1f;

    // 2. 조명이 적용된 색상을 finalColor로 덮어씌움
    float4 finalColor = float4(litColor, baseColor.a);

    for (uint i = 0; i < LocalTintCount; ++i)
    {
        float radius = LocalTints[i].PositionRadius.w;
        if (radius <= 0.0f)
        {
            continue;
        }

        //현재 픽셀과 로컬 틴트 중심 사이의 거리를 계산하여, 반경 내에서만 틴트 효과가 적용되도록 함
        float distanceToTintCenter = distance(input.worldPos, LocalTints[i].PositionRadius.xyz);
        float normalizedDistance = saturate(distanceToTintCenter / max(radius, 0.0001f));
        
        //멀어질 수록 약하게 작용
        float attenuation = pow(saturate(1.0f - normalizedDistance), max(LocalTints[i].Params.y, 0.0001f));
        float localTintWeight = saturate(LocalTints[i].Params.x * attenuation);
        finalColor.rgb = lerp(finalColor.rgb, LocalTints[i].Color.rgb, localTintWeight);
    }

    finalColor.rgb = saturate(finalColor.rgb);
    finalColor.a = texColor.a * input.color.a;


    /*float heat = saturate((float) lightCountInTile / 10.0f);
    
    // 타일 경계선을 시각적으로 확인하고 싶다면 덤프 추가
    bool bIsBorder = (pixelPos.x % TILE_SIZE == 0) || (pixelPos.y % TILE_SIZE == 0);
    if (bIsBorder)
        return float4(0, 1, 0, 1); // 타일 경계는 초록색 선으로 출력

    return float4(heat, 0.0f, 0.0f, 1.0f); // 빛 개수에 비례한 붉은색 출력*/

    // ✂️ 기존 리턴문은 잠시 주석 처리
    return float4(ApplyWireframe(finalColor.rgb), finalColor.a);
}

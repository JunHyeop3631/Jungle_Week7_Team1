#include "Common/Functions.hlsl"
#include "Common/VertexLayouts.hlsl"

// 컬러 PNG/TGA 텍스처를 단일 quad에 그리는 빌보드 전용 셰이더.
// SubUV 와 다르게 R 채널이 아닌 알파 채널만으로 컷오프 판정한다.
Texture2D BillboardTex : register(t0);
SamplerState BillboardSampler : register(s0);

PS_Input_Full VS(VS_Input_PNCT input)
{
    PS_Input_Full output;
    output.position = ApplyVP(input.position);
    output.texcoord = input.texcoord;
    output.color = input.color;
    return output;
}

float4 PS(PS_Input_Full input) : SV_TARGET
{
    float4 col = BillboardTex.Sample(BillboardSampler, input.texcoord);

    // 알파 컷오프 (straight alpha PNG의 보간 헤일로 차단)
    if (!bIsWireframe && col.a < 0.5f)
        discard;
    col = col * input.color;
    return float4(ApplyWireframe(col.rgb), bIsWireframe ? 1.0f : col.a);
}

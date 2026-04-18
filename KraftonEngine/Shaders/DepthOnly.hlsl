#include "Common/Functions.hlsl"
#include "Common/VertexLayouts.hlsl"

PS_Input_PosOnly VS(VS_Input_P input)
{
    PS_Input_PosOnly output;
    output.position = ApplyMVP(input.position);
    return output;
}
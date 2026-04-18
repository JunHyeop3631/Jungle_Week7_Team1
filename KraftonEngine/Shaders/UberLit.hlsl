#include "Common/Functions.hlsl"
#include "Common/VertexLayouts.hlsl"
#include "Common/ConstantBuffers.hlsl"

Texture2D g_txColor : register(t0);
Texture2D g_txNormal : register(t1);
SamplerState g_Sample : register(s0);

PS_Lighting VS(VS_Input_PNCT input)
{
    PS_Lighting output;
    output.position = ApplyMVP(input.position);;
    output.worldPosition = mul(float4(input.position, 1.0f), Model).xyz;
    output.texCoord = input.texcoord;
    
    output.worldNormal = normalize(mul(input.normal, (float3x3) NormalMatrix));
    float3 worldTanXYZ = normalize(mul(input.tangent.xyz, (float3x3) Model));
    output.worldTangent = normalize(mul(input.tangent.rgb, (float3x3) Model));
    
//    //구루 쉐이딩
#if LIGHTING_MODEL_GOURAUD
    float3 AmbientColor = 0;
    LightingResult lightingResult = (LightingResult) 0;
    
    AmbientColor = Ambient.LightColor * 0.1f; // 간단한 앰비언트 조명
    float shininess = SpecularRoughness;
    
    lightingResult += ComputeDirectionalLight_BlinnPhong(CameraPosition.xyz, output.worldPosition, output.worldNormal, shininess);
    lightingResult += ComputePointLight_BlinnPhong(CameraPosition.xyz, output.worldPosition, output.worldNormal, shininess);
    lightingResult += ComputeSpotLight_BlinnPhong(CameraPosition.xyz, output.worldPosition, output.worldNormal, shininess);
    
    output.vertexLighting = AmbientColor + lightingResult.Diffuse + lightingResult.Specular;
#endif
    output.color = input.color;
    
    return output;
}

float4 PS(PS_Lighting input)
{
    float4 finalColor;
    
    float4 texColor = g_txColor.Sample(g_Sample, input.texCoord) * SectionColor;
    float3 worldNormal = normalize(input.worldNormal);
    
    if (bHasNormalMap != 0)
        worldNormal = GetWorldNormal(input, g_txNormal, g_Sample);
    
#if VIEWMODE_NORMAL
    return float4(worldNormal * 0.5f + 0.5f, 1.0f);
#endif
    LightingResult lightingResult = (LightingResult) 0;
    //고로쉐이딩
#if LIGHTING_MODEL_GOURAUD
    finalColor = texColor * input.color * float4(input.vertexLighting, 1.0f);   
#elif LIGHTING_MODEL_LAMBERT
    lightingResult += ComputeDirectionalLight_Lambert(worldNormal);
    lightingResult += ComputePointLight_Lambert(input.worldPosition, worldNormal);
    lightingResult += ComputeSpotLight_Lambert(input.worldPosition, worldNormal);
    
    float3 albedo = texColor.rgb * input.color.rgb;

    float3 ambient = Ambient.LightColor.rgb * 0.1f * albedo;
    float3 diffuse  = lightingResult.Diffuse * albedo * SectionColor.rgb;

    float3 final = ambient + diffuse;

    finalColor = float4(final, input.color.a * texColor.a);
    
#elif LIGHTING_MODEL_PHONG
    float shininess = SpecularRoughness;
    lightingResult += ComputeDirectionalLight_BlinnPhong(CameraPosition.xyz, input.worldPosition, worldNormal, shininess);
    lightingResult += ComputePointLight_BlinnPhong(CameraPosition.xyz, input.worldPosition, worldNormal, shininess);
    lightingResult += ComputeSpotLight_BlinnPhong(CameraPosition.xyz, input.worldPosition, worldNormal, shininess);
    
    float3 albedo = texColor.rgb * input.color.rgb;
    
    float3 ambient = Ambient.LightColor.rgb * 0.1f * albedo;
    float3 diffuse  = lightingResult.Diffuse * albedo * SectionColor.rgb;
    float3 specular = lightingResult.Specular;

    float3 final = ambient + diffuse + specular;

    finalColor = float4(final, texColor.a);
#elif LIGHTING_MODEL_UNLIT
    finalColor = texColor * input.color;
#endif
    
    return finalColor;
}
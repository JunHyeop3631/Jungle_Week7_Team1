#include "Common/Functions.hlsl"
#include "Common/VertexLayouts.hlsl"
#include "Common/ConstantBuffers.hlsl"

#if !defined(LIGHTING_MODEL_GOURAUD) && !defined(LIGHTING_MODEL_LAMBERT) && !defined(LIGHTING_MODEL_PHONG) && !defined(LIGHTING_MODEL_UNLIT)
#define LIGHTING_MODEL_UNLIT 1
#endif

Texture2D g_txColor : register(t0);
Texture2D g_txNormal : register(t1);
SamplerState g_Sample : register(s0);

PS_Lighting VS(VS_Input_PNCT input)
{
    PS_Lighting output = (PS_Lighting) 0;
    
    output.position = ApplyMVP(input.position);
    output.worldPosition = mul(float4(input.position, 1.0f), Model).xyz;
    output.texCoord = input.texcoord;
    
    output.worldNormal = normalize(mul(input.normal, (float3x3) NormalMatrix));
    float3 worldTanXYZ = normalize(mul(input.tangent.xyz, (float3x3) Model));
    output.worldTangent = float4(worldTanXYZ, input.tangent.w);
    output.color = input.color;
    
// 구루 쉐이딩 (VS 단계이므로 타일 컬링 없이 _NoTile 함수 사용)
#if LIGHTING_MODEL_GOURAUD
    float3 AmbientColor = Ambient.LightColor.rgb * 0.1f;
    float shininess = SpecularRoughness;
    
    LightingResult totalLighting = (LightingResult)0;
    LightingResult tempLighting = (LightingResult)0;
    
    tempLighting = ComputeDirectionalLight_BlinnPhong(CameraPosition.xyz, output.worldPosition, output.worldNormal, shininess);
    totalLighting.Diffuse += tempLighting.Diffuse;
    totalLighting.Specular += tempLighting.Specular;
    
    tempLighting = ComputePointLight_BlinnPhong_NoTile(CameraPosition.xyz, output.worldPosition, output.worldNormal, shininess);
    totalLighting.Diffuse += tempLighting.Diffuse;
    totalLighting.Specular += tempLighting.Specular;
    
    tempLighting = ComputeSpotLight_BlinnPhong_NoTile(CameraPosition.xyz, output.worldPosition, output.worldNormal, shininess);
    totalLighting.Diffuse += tempLighting.Diffuse;
    totalLighting.Specular += tempLighting.Specular;
    
    output.vertexLighting = AmbientColor + totalLighting.Diffuse + (totalLighting.Specular * SpecularIntensity);
#endif
    
    return output;
}

float4 PS(PS_Lighting input) : SV_TARGET
{
    float4 finalColor = float4(1.0f, 0.0f, 1.0f, 1.0f);
    float4 texColor = g_txColor.Sample(g_Sample, input.texCoord) * SectionColor;
    if (texColor.a < 0.001f)
    {
        texColor = float4(1.0f, 1.0f, 1.0f, 1.0f) * SectionColor;
    }
    float3 worldNormal = normalize(input.worldNormal);
    
    if (bHasNormalMap != 0)
        worldNormal = GetWorldNormal(input, g_txNormal, g_Sample);
    
#if VIEWMODE_NORMAL
    return float4(worldNormal * 0.5f + 0.5f, 1.0f);
#endif
    
    LightingResult totalLighting = (LightingResult) 0;
    LightingResult tempLighting = (LightingResult) 0;
    
// 고로 쉐이딩
#if LIGHTING_MODEL_GOURAUD
    finalColor = texColor * input.color * float4(input.vertexLighting, 1.0f);
    
// 램버트 쉐이딩 (PS 단계이므로 input.position.xy 를 넘겨 타일 컬링 적용)
#elif LIGHTING_MODEL_LAMBERT
    tempLighting = ComputeDirectionalLight_Lambert(worldNormal);
    totalLighting.Diffuse += tempLighting.Diffuse;
    
    tempLighting = ComputePointLight_Lambert(input.worldPosition, worldNormal, input.position.xy);
    totalLighting.Diffuse += tempLighting.Diffuse;
    
    tempLighting = ComputeSpotLight_Lambert(input.worldPosition, worldNormal, input.position.xy);
    totalLighting.Diffuse += tempLighting.Diffuse;
    
    float3 albedo = texColor.rgb * input.color.rgb;
    float3 ambient = Ambient.LightColor.rgb * 0.1f * albedo;
    float3 diffuse  = totalLighting.Diffuse * albedo;
    float3 final = ambient + diffuse;
    finalColor = float4(final, input.color.a * texColor.a);
    
// 블린 폰 쉐이딩 (PS 단계이므로 input.position.xy 를 넘겨 타일 컬링 적용)
#elif LIGHTING_MODEL_PHONG
    float shininess = SpecularRoughness;
    
    tempLighting = ComputeDirectionalLight_BlinnPhong(CameraPosition.xyz, input.worldPosition, worldNormal, shininess);
    totalLighting.Diffuse += tempLighting.Diffuse;
    totalLighting.Specular += tempLighting.Specular;
    
    tempLighting = ComputePointLight_BlinnPhong(CameraPosition.xyz, input.worldPosition, worldNormal, shininess, input.position.xy);
    totalLighting.Diffuse += tempLighting.Diffuse;
    totalLighting.Specular += tempLighting.Specular;
    
    tempLighting = ComputeSpotLight_BlinnPhong(CameraPosition.xyz, input.worldPosition, worldNormal, shininess, input.position.xy);
    totalLighting.Diffuse += tempLighting.Diffuse;
    totalLighting.Specular += tempLighting.Specular;
    
    float3 albedo = texColor.rgb * input.color.rgb;
    float3 ambient = Ambient.LightColor.rgb * 0.1f * albedo;
    float3 diffuse = totalLighting.Diffuse * albedo;
    float3 specular = totalLighting.Specular * SpecularIntensity;
    
    float3 final = ambient + diffuse + specular;
    finalColor = float4(final, input.color.a * texColor.a);
    
 // 언릿 (조명 없음)
#elif LIGHTING_MODEL_UNLIT
    finalColor = texColor * input.color;
#endif
    
    
    return finalColor;
}
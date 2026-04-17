#ifndef CONSTANT_BUFFERS_HLSL
#define CONSTANT_BUFFERS_HLSL

#pragma pack_matrix(row_major)

// b0: 프레임 공통 — ViewProj, 와이어프레임 설정
cbuffer FrameBuffer : register(b0)
{
    float4x4 View;
    float4x4 Projection;
    float bIsWireframe;
    float3 WireframeRGB;
    float Time;
    float NearPlane;
    float FarPlane;
    float _framePad0;
    float3 CameraPosition;
    float _framePad1;
    
    float4x4 InverseView;
    float4x4 InverseProjection;
    float4x4 InverseViewProjection;
    
    float InvDeviceZToWorldZTransform2;
    float InvDeviceZToWorldZTransform3;
    float _framePad2[2];
}

struct LocalTintEffectData
{
    float4 PositionRadius;
    float4 Color;
    float4 Params;
};

cbuffer SceneEffectBuffer : register(b5)
{
    LocalTintEffectData LocalTints[8];
    uint LocalTintCount;
    float3 _sceneEffectPad;
}

// b1: 오브젝트별 — 월드 변환, 색상
cbuffer PerObjectBuffer : register(b1)
{
    float4x4 Model;
    float4 PrimitiveColor;
};

// b2: 기즈모 전용
cbuffer GizmoBuffer : register(b2)
{
    float4 GizmoColorTint;
    uint bIsInnerGizmo;
    uint bClicking;
    uint SelectedAxis;
    float HoveredAxisOpacity;
    uint AxisMask; // 비트 0=X, 1=Y, 2=Z
    uint bOverrideAxisColor;
    uint2 _gizmoPad;
};

// ── Outline 설정 (b3) ──
cbuffer OutlinePostProcessCB : register(b3)
{
    float4 OutlineColor; // 아웃라인 색상 + 알파
    float OutlineThickness; // 샘플링 오프셋 (픽셀 단위, 보통 1.0)
    float OutlineFalloff;  // Radius 감쇠 지수
    float bOutputLumaToAlpha;
    float OutputAlpha;
};

struct FogUniformParameters
{
    float4 ExponentialFogParameters;
    float4 ExponentialFogColorParameter;
    float4 ExponentialFogParameters3;
};

cbuffer FogPostProcessCB : register(b6)
{
    FogUniformParameters Fogs[8];
    uint FogCount;
    float3 _FogPad;
}

// b4: Material properties
cbuffer MaterialBuffer : register(b4)
{
    uint bIsUVScroll;
    uint bHasNormalMap; // 노멀맵 사용 여부
    float SpecularRoughness; // Blinn-Phong Shininess
    float SpecularIntensity; // 반사광 세기
    float4 SectionColor;
}

// b7: ID picking
cbuffer PickingBuffer : register(b7)
{
    uint PickingId;
    float3 _pickPad;
}

#define NUM_POINT_LIGHT 4
#define NUM_SPOT_LIGHT 4

struct FAmbientLightInfo
{
    float4 LightColor;
};

struct FDirectionalLightInfo
{
    float4 LightColor;
    float4 Direction;
};

struct FPointLightInfo
{
    float4 LightColor;
    float4 Position;
    float AttenuationRadius;
    float FalloffExponent;
    float2 pad; // 16바이트 정렬을 위한 패딩
};

struct FSpotLightInfo
{
    float4 LightColor;
    float4 Position;
    float4 Direction;
    float AttenuationRadius;
    float InnerConeAngle;
    float OuterConeAngle;
    float pad; // 16바이트 정렬을 위한 패딩
};

// [수정 완료] 팀의 원래 기획 + 엔진의 b8 슬롯 결합
cbuffer LightingBuffer : register(b8)
{
    FAmbientLightInfo Ambient;
    FDirectionalLightInfo Directional;
    FPointLightInfo PointLights[NUM_POINT_LIGHT];
    FSpotLightInfo SpotLights[NUM_SPOT_LIGHT];
};

#endif // CONSTANT_BUFFERS_HLSL

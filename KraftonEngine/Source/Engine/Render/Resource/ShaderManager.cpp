#include "ShaderManager.h"
#include "Render/Types/VertexTypes.h"

namespace
{
uint32 ToStaticMeshLightingShaderIndex(EViewMode InViewMode)
{
	switch (InViewMode)
	{
	case EViewMode::Lit_Gouraud:
		return 0;
	case EViewMode::Lit_Lambert:
		return 1;
	case EViewMode::Lit_Phong:
		return 2;
	case EViewMode::Unlit:
	default:
		return 3;
	}
}
}

void FShaderManager::Initialize(ID3D11Device* InDevice)
{
	if (bIsInitialized) return;

	Shaders[(uint32)EShaderType::Primitive].Create(InDevice, L"Shaders/Primitive.hlsl",
		"VS", "PS", FVertexInputLayout, ARRAYSIZE(FVertexInputLayout));

	Shaders[(uint32)EShaderType::Gizmo].Create(InDevice, L"Shaders/Gizmo.hlsl",
		"VS", "PS", FVertexInputLayout, ARRAYSIZE(FVertexInputLayout));

	Shaders[(uint32)EShaderType::Editor].Create(InDevice, L"Shaders/Editor.hlsl",
		"VS", "PS", FVertexInputLayout, ARRAYSIZE(FVertexInputLayout));

	Shaders[(uint32)EShaderType::StaticMesh].Create(InDevice, L"Shaders/UberLit.hlsl",
     "VS", "PS", FVertexPNCTInputLayout, ARRAYSIZE(FVertexPNCTInputLayout),
		FShader::GetLightingModelShaderMacro(EViewMode::Unlit));

	StaticMeshLightingShaders[ToStaticMeshLightingShaderIndex(EViewMode::Lit_Gouraud)].Create(
		InDevice,
		L"Shaders/UberLit.hlsl",
		"VS",
		"PS",
		FVertexPNCTInputLayout,
		ARRAYSIZE(FVertexPNCTInputLayout),
		FShader::GetLightingModelShaderMacro(EViewMode::Lit_Gouraud));

	StaticMeshLightingShaders[ToStaticMeshLightingShaderIndex(EViewMode::Lit_Lambert)].Create(
		InDevice,
		L"Shaders/UberLit.hlsl",
		"VS",
		"PS",
		FVertexPNCTInputLayout,
		ARRAYSIZE(FVertexPNCTInputLayout),
		FShader::GetLightingModelShaderMacro(EViewMode::Lit_Lambert));

	StaticMeshLightingShaders[ToStaticMeshLightingShaderIndex(EViewMode::Lit_Phong)].Create(
		InDevice,
		L"Shaders/UberLit.hlsl",
		"VS",
		"PS",
		FVertexPNCTInputLayout,
		ARRAYSIZE(FVertexPNCTInputLayout),
		FShader::GetLightingModelShaderMacro(EViewMode::Lit_Phong));

	StaticMeshLightingShaders[ToStaticMeshLightingShaderIndex(EViewMode::Unlit)].Create(
		InDevice,
		L"Shaders/UberLit.hlsl",
		"VS",
		"PS",
		FVertexPNCTInputLayout,
		ARRAYSIZE(FVertexPNCTInputLayout),
		FShader::GetLightingModelShaderMacro(EViewMode::Unlit));

	Shaders[(uint32)EShaderType::Decal].Create(InDevice, L"Shaders/Decal.hlsl",
		"VS", "PS", FVertexInputLayout, ARRAYSIZE(FVertexInputLayout));

	// PostProcess outline: fullscreen quad (InputLayout 없음)
	Shaders[(uint32)EShaderType::OutlinePostProcess].Create(InDevice, L"Shaders/OutlinePostProcess.hlsl",
		"VS", "PS", nullptr, 0);

	Shaders[(uint32)EShaderType::FXAAPostProcess].Create(InDevice, L"Shaders/FXAA.hlsl",
		"VS", "PS", nullptr, 0);

	Shaders[(uint32)EShaderType::FogPostProcess].Create(InDevice, L"Shaders/FogPostProcess.hlsl",
		"VS", "PS", nullptr, 0);

	// Batcher 셰이더 (FTextureVertex: POSITION + TEXCOORD)
	Shaders[(uint32)EShaderType::DepthView].Create(InDevice, L"Shaders/SceneDepthVisualize.hlsl",
		"VS", "PS", nullptr, 0);

	Shaders[(uint32)EShaderType::Font].Create(InDevice, L"Shaders/ShaderFont.hlsl",
		"VS", "PS", FTextureVertexInputLayout, ARRAYSIZE(FTextureVertexInputLayout));

	Shaders[(uint32)EShaderType::OverlayFont].Create(InDevice, L"Shaders/ShaderOverlayFont.hlsl",
		"VS", "PS", FTextureVertexInputLayout, ARRAYSIZE(FTextureVertexInputLayout));

	Shaders[(uint32)EShaderType::SubUV].Create(InDevice, L"Shaders/ShaderSubUV.hlsl",
		"VS", "PS", FTextureVertexInputLayout, ARRAYSIZE(FTextureVertexInputLayout));

	Shaders[(uint32)EShaderType::Billboard].Create(InDevice, L"Shaders/ShaderBillboard.hlsl",
		"VS", "PS", FVertexPNCTInputLayout, ARRAYSIZE(FVertexPNCTInputLayout));

	Shaders[(uint32)EShaderType::IDPickPrimitive].Create(InDevice, L"Shaders/IDPick.hlsl",
		"VS_PC", "PS_Primitive", FVertexInputLayout, ARRAYSIZE(FVertexInputLayout));

	Shaders[(uint32)EShaderType::IDPickBillboard].Create(InDevice, L"Shaders/IDPick.hlsl",
		"VS_Billboard", "PS_BillboardCutout", FVertexInputLayout, ARRAYSIZE(FVertexInputLayout));

	Shaders[(uint32)EShaderType::IDPickStaticMesh].Create(InDevice, L"Shaders/IDPick.hlsl",
		"VS_PNCT", "PS_TexturedCutout", FVertexPNCTInputLayout, ARRAYSIZE(FVertexPNCTInputLayout));

	Shaders[(uint32)EShaderType::IDPickDebugVisualize].Create(InDevice, L"Shaders/IDPickDebug.hlsl",
		"VS", "PS", nullptr, 0);
	Shaders[(uint32)EShaderType::LightCullingCS_Point].CreateCompute(InDevice, L"Shaders/LightCulling.hlsl",
		"CS_Point");
	Shaders[(uint32)EShaderType::LightCullingCS_Spot].CreateCompute(InDevice, L"Shaders/LightCulling.hlsl",
		"CS_Spot");
	bIsInitialized = true;
}

void FShaderManager::Release()
{
	for (uint32 i = 0; i < (uint32)EShaderType::MAX; ++i)
	{
		Shaders[i].Release();
	}

	for (uint32 i = 0; i < StaticMeshLightingShaderCount; ++i)
	{
		StaticMeshLightingShaders[i].Release();
	}

	bIsInitialized = false;
}

FShader* FShaderManager::GetShader(EShaderType InType)
{
	uint32 Idx = (uint32)InType;
	if (Idx < (uint32)EShaderType::MAX)
	{
		return &Shaders[Idx];
	}
	return nullptr;
}

FShader* FShaderManager::GetStaticMeshShader(EViewMode InViewMode)
{
	return &StaticMeshLightingShaders[ToStaticMeshLightingShaderIndex(InViewMode)];
}

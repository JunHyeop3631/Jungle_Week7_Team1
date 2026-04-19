#pragma once

#include "Core/Singleton.h"
#include "Render/Resource/Shader.h"
#include "Core/CoreTypes.h"

enum class EShaderType : uint32
{
	Primitive,
	Gizmo,
	Editor,
	StaticMesh,
    Decal,
	//	SelectionMask 전용(OutlineMask RT 기록용)
	SelectionMask,
	//	PostProcess 체인 셰이더
	OutlinePostProcess,
	FogPostProcess,
	FXAAPostProcess,
	//	ViewMode 분기용 깊이 디버그 셰이더
	DepthView,
	Font,
	OverlayFont,
	SubUV,
	Billboard,
	IDPickPrimitive,
	IDPickBillboard,
	IDPickStaticMesh,
	IDPickDebugVisualize,
	LightCullingCS_Point,
	LightCullingCS_Spot,
	MAX,
};

class FShaderManager : public TSingleton<FShaderManager>
{
	friend class TSingleton<FShaderManager>;

public:
	void Initialize(ID3D11Device* InDevice);
	void Release();

	FShader* GetShader(EShaderType InType);
	FShader* GetStaticMeshShader(EViewMode InViewMode);

private:
	FShaderManager() = default;
	static constexpr uint32 StaticMeshLightingShaderCount = 4;

	FShader Shaders[(uint32)EShaderType::MAX];
    FShader StaticMeshLightingShaders[StaticMeshLightingShaderCount];
	bool bIsInitialized = false;
};

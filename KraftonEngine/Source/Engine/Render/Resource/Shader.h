#pragma once
#include "Render/Types/RenderTypes.h"
#include "Render/Types/ViewTypes.h"
#include "Core/CoreTypes.h"

#include <string>
#include <filesystem>
#include <vector>

class FShader
{
public:
	FShader() = default;
	~FShader() { Release(); }

	void CheckAndHotReload(ID3D11Device* InDevice);

	static void SetCurrentLightingViewMode(EViewMode InViewMode);
	static EViewMode GetCurrentLightingViewMode();
	static bool IsLightingModelViewMode(EViewMode InViewMode);
	static const D3D_SHADER_MACRO* GetLightingModelShaderMacro(EViewMode InViewMode);

	FShader(const FShader&) = delete;
	FShader& operator=(const FShader&) = delete;
	FShader(FShader&& Other) noexcept;
	FShader& operator=(FShader&& Other) noexcept;

	void Create(ID3D11Device* InDevice, const wchar_t* InFilePath, const char* InVSEntryPoint, const char* InPSEntryPoint,
		const D3D11_INPUT_ELEMENT_DESC* InInputElements, uint32 InInputElementCount,
		const D3D_SHADER_MACRO* InDefines = nullptr);
	void CreateCompute(ID3D11Device* InDevice, const wchar_t* InFilePath, const char* InCSEntryPoint, const D3D_SHADER_MACRO* InDefines = nullptr);

	void Release();

	void Bind(ID3D11DeviceContext* InDeviceContext) const;
	void BindCompute(ID3D11DeviceContext* InDeviceContext) const;

private:
	ID3D11VertexShader* VertexShader = nullptr;
	ID3D11PixelShader* PixelShader = nullptr;
	ID3D11ComputeShader* ComputeShader = nullptr;
	ID3D11InputLayout* InputLayout = nullptr;

	size_t CachedVertexShaderSize = 0;
	size_t CachedPixelShaderSize = 0;
	size_t CachedComputeShaderSize = 0;

	// === 핫 리로드용 캐시 데이터 ===
	std::wstring CachedFilePath;
	std::string CachedVSEntry;
	std::string CachedPSEntry;
	std::string CachedCSEntry; // 컴퓨트 셰이더 엔트리 포인트 캐싱용 추가
	bool bIsComputeShader = false; // 컴퓨트 셰이더 여부 구분용 추가

	// 포인터 배열을 안전하게 보관하기 위한 vector
	std::vector<D3D11_INPUT_ELEMENT_DESC> CachedInputElements;
	std::vector<D3D_SHADER_MACRO> CachedDefines;

	std::filesystem::file_time_type LastWriteTime;
	bool bCanHotReload = false;
};

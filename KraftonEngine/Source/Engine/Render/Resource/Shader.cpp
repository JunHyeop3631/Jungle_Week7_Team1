#include "Shader.h"
#include "Profiling/MemoryStats.h"
#include <string_view>

static D3D_SHADER_MACRO Defines_Gouraud[] =
{
	{ "LIGHTING_MODEL_GOURAUD", "1" },
   { nullptr, nullptr }
};

static D3D_SHADER_MACRO Defines_Phong[] =
{
	{ "LIGHTING_MODEL_PHONG", "1" },
   { nullptr, nullptr }
};

static D3D_SHADER_MACRO Defines_Lambert[] =
{
	{ "LIGHTING_MODEL_LAMBERT", "1" },
   { nullptr, nullptr }
};

static D3D_SHADER_MACRO Defines_Unlit[] =
{
	{ "LIGHTING_MODEL_UNLIT", "1" },
	{ nullptr, nullptr }
};

static D3D_SHADER_MACRO Defines_WorldNormal[] =
{
	{ "VIEWMODE_NORMAL", "1" },
	{ nullptr, nullptr }
};

static EViewMode GLightingViewMode = EViewMode::Unlit;

void FShader::CheckAndHotReload(ID3D11Device* InDevice)
{
	if (!bCanHotReload || CachedFilePath.empty()) return;

	const D3D_SHADER_MACRO* DefinesPtr = CachedDefines.empty() ? nullptr : CachedDefines.data();
	ID3DBlob* errorBlob = nullptr;

	if (bIsComputeShader)
	{
		// === 컴퓨트 셰이더(CS) 리로드 로직 ===
		ID3DBlob* newCS_CSO = nullptr;
		HRESULT hr = D3DCompileFromFile(CachedFilePath.c_str(), DefinesPtr, D3D_COMPILE_STANDARD_FILE_INCLUDE, CachedCSEntry.c_str(), "cs_5_0", 0, 0, &newCS_CSO, &errorBlob);

		if (FAILED(hr))
		{
			if (errorBlob)
			{
				std::string ErrorMsg = "HotReload CS Error in " + std::string(CachedFilePath.begin(), CachedFilePath.end()) + "\n";
				ErrorMsg += (char*)errorBlob->GetBufferPointer();
				ErrorMsg += "\n";
				OutputDebugStringA(ErrorMsg.c_str());
				errorBlob->Release();
			}
			return; // 실패 시 기존 셰이더 유지
		}

		// 컴파일 성공 시 기존 리소스 해제 후 교체
		Release();

		InDevice->CreateComputeShader(newCS_CSO->GetBufferPointer(), newCS_CSO->GetBufferSize(), nullptr, &ComputeShader);
		CachedComputeShaderSize = newCS_CSO->GetBufferSize();
		MemoryStats::AddComputeShaderMemory(static_cast<uint32>(CachedComputeShaderSize));

		newCS_CSO->Release();

		bCanHotReload = true;
		bIsComputeShader = true; // Release() 호출 시 초기화될 수 있으므로 다시 true 설정

		std::wstring SuccessMsg = L"[ShaderManager] Successfully hot-reloaded CS: " + CachedFilePath + L"\n";
		OutputDebugStringW(SuccessMsg.c_str());
	}
	else
	{
		// === 버텍스/픽셀 셰이더(VS/PS) 리로드 로직 ===
		ID3DBlob* newVS_CSO = nullptr;
		ID3DBlob* newPS_CSO = nullptr;

		// VS 컴파일 시도
		HRESULT hr = D3DCompileFromFile(CachedFilePath.c_str(), DefinesPtr, D3D_COMPILE_STANDARD_FILE_INCLUDE, CachedVSEntry.c_str(), "vs_5_0", 0, 0, &newVS_CSO, &errorBlob);
		if (FAILED(hr))
		{
			if (errorBlob)
			{
				std::string ErrorMsg = "HotReload VS Error in " + std::string(CachedFilePath.begin(), CachedFilePath.end()) + "\n";
				ErrorMsg += (char*)errorBlob->GetBufferPointer();
				ErrorMsg += "\n";
				OutputDebugStringA(ErrorMsg.c_str());
				errorBlob->Release();
			}
			return; // 실패 시 기존 셰이더 유지
		}

		// PS 컴파일 시도
		hr = D3DCompileFromFile(CachedFilePath.c_str(), DefinesPtr, D3D_COMPILE_STANDARD_FILE_INCLUDE, CachedPSEntry.c_str(), "ps_5_0", 0, 0, &newPS_CSO, &errorBlob);
		if (FAILED(hr))
		{
			if (errorBlob)
			{
				std::string ErrorMsg = "HotReload PS Error in " + std::string(CachedFilePath.begin(), CachedFilePath.end()) + "\n";
				ErrorMsg += (char*)errorBlob->GetBufferPointer();
				ErrorMsg += "\n";
				OutputDebugStringA(ErrorMsg.c_str());
				errorBlob->Release();
			}
			newVS_CSO->Release();
			return; // 실패 시 기존 셰이더 유지
		}

		// === 컴파일 성공! 기존 리소스 해제 후 새 리소스로 교체 ===
		Release(); // 기존 VS, PS, InputLayout 해제 (MemoryStats 반영 포함)

		InDevice->CreateVertexShader(newVS_CSO->GetBufferPointer(), newVS_CSO->GetBufferSize(), nullptr, &VertexShader);
		CachedVertexShaderSize = newVS_CSO->GetBufferSize();
		MemoryStats::AddVertexShaderMemory(static_cast<uint32>(CachedVertexShaderSize));

		InDevice->CreatePixelShader(newPS_CSO->GetBufferPointer(), newPS_CSO->GetBufferSize(), nullptr, &PixelShader);
		CachedPixelShaderSize = newPS_CSO->GetBufferSize();
		MemoryStats::AddPixelShaderMemory(static_cast<uint32>(CachedPixelShaderSize));

		if (!CachedInputElements.empty())
		{
			InDevice->CreateInputLayout(CachedInputElements.data(), static_cast<UINT>(CachedInputElements.size()), newVS_CSO->GetBufferPointer(), newVS_CSO->GetBufferSize(), &InputLayout);
		}

		newVS_CSO->Release();
		newPS_CSO->Release();

		bCanHotReload = true;

		std::wstring SuccessMsg = L"[ShaderManager] Successfully hot-reloaded: " + CachedFilePath + L"\n";
		OutputDebugStringW(SuccessMsg.c_str());
	}
}

void FShader::SetCurrentLightingViewMode(EViewMode InViewMode)
{
	if (IsLightingModelViewMode(InViewMode))
	{
		GLightingViewMode = InViewMode;
	}
}

EViewMode FShader::GetCurrentLightingViewMode()
{
	return GLightingViewMode;
}

bool FShader::IsLightingModelViewMode(EViewMode InViewMode)
{
	return InViewMode == EViewMode::Lit_Gouraud
		|| InViewMode == EViewMode::Lit_Lambert
		|| InViewMode == EViewMode::Lit_Phong
		|| InViewMode == EViewMode::Unlit;
}

const D3D_SHADER_MACRO* FShader::GetLightingModelShaderMacro(EViewMode InViewMode)
{
	switch (InViewMode)
	{
	case EViewMode::Lit_Gouraud:
		return Defines_Gouraud;
	case EViewMode::Lit_Lambert:
		return Defines_Lambert;
	case EViewMode::Lit_Phong:
		return Defines_Phong;
	case EViewMode::WorldNormal:
		return Defines_WorldNormal;
	case EViewMode::Unlit:
	default:
		return Defines_Unlit;
	}
}

FShader::FShader(FShader&& Other) noexcept
	: VertexShader(Other.VertexShader)
	, PixelShader(Other.PixelShader)
	, InputLayout(Other.InputLayout)
	, CachedVertexShaderSize(Other.CachedVertexShaderSize)
	, CachedPixelShaderSize(Other.CachedPixelShaderSize)
{
	Other.VertexShader = nullptr;
	Other.PixelShader = nullptr;
	Other.InputLayout = nullptr;
	Other.CachedVertexShaderSize = 0;
	Other.CachedPixelShaderSize = 0;
}

FShader& FShader::operator=(FShader&& Other) noexcept
{
	if (this != &Other)
	{
		Release();
		VertexShader = Other.VertexShader;
		PixelShader = Other.PixelShader;
		InputLayout = Other.InputLayout;
		CachedVertexShaderSize = Other.CachedVertexShaderSize;
		CachedPixelShaderSize = Other.CachedPixelShaderSize;
		Other.VertexShader = nullptr;
		Other.PixelShader = nullptr;
		Other.InputLayout = nullptr;
		Other.CachedVertexShaderSize = 0;
		Other.CachedPixelShaderSize = 0;
	}
	return *this;
}

void FShader::Create(ID3D11Device* InDevice, const wchar_t* InFilePath, const char* InVSEntryPoint, const char* InPSEntryPoint,
	const D3D11_INPUT_ELEMENT_DESC* InInputElements, UINT InInputElementCount,
	const D3D_SHADER_MACRO* InDefines)
{
	Release();

	// 1. [핵심] UberLit 하드코딩 제거 및 매크로 그대로 사용
	const D3D_SHADER_MACRO* ShaderDefines = InDefines;

	// 2. 핫 리로드를 위한 데이터 캐싱
	CachedFilePath = InFilePath ? InFilePath : L"";
	CachedVSEntry = InVSEntryPoint ? InVSEntryPoint : "";
	CachedPSEntry = InPSEntryPoint ? InPSEntryPoint : "";

	CachedInputElements.clear();

	if (InInputElements && InInputElementCount > 0)
	{
		CachedInputElements.assign(InInputElements, InInputElements + InInputElementCount);
	}

	CachedDefines.clear();
	if (InDefines)
	{
		const D3D_SHADER_MACRO* Macro = InDefines;
		while (Macro->Name != nullptr)
		{
			CachedDefines.push_back(*Macro);
			Macro++;
		}
		CachedDefines.push_back({ nullptr, nullptr }); // Null-terminator 필수
	}

	bCanHotReload = true;

	ID3DBlob* vertexShaderCSO = nullptr;
	ID3DBlob* pixelShaderCSO = nullptr;
	ID3DBlob* errorBlob = nullptr;

	// Vertex Shader 컴파일
    HRESULT hr = D3DCompileFromFile(InFilePath, ShaderDefines, D3D_COMPILE_STANDARD_FILE_INCLUDE, InVSEntryPoint, "vs_5_0", 0, 0, &vertexShaderCSO, &errorBlob);
	if (FAILED(hr))
	{
		if (errorBlob)
		{
			MessageBoxA(nullptr, (char*)errorBlob->GetBufferPointer(), "Vertex Shader Compile Error", MB_OK | MB_ICONERROR);
			errorBlob->Release();
		}
		return;
	}

	// Pixel Shader 컴파일
	hr = D3DCompileFromFile(InFilePath, ShaderDefines, D3D_COMPILE_STANDARD_FILE_INCLUDE, InPSEntryPoint, "ps_5_0", 0, 0, &pixelShaderCSO, &errorBlob);
	if (FAILED(hr))
	{
		if (errorBlob)
		{
			MessageBoxA(nullptr, (char*)errorBlob->GetBufferPointer(), "Pixel Shader Compile Error", MB_OK | MB_ICONERROR);
			errorBlob->Release();
		}
		vertexShaderCSO->Release();
		return;
	}

	// Vertex Shader 생성
	hr = InDevice->CreateVertexShader(vertexShaderCSO->GetBufferPointer(), vertexShaderCSO->GetBufferSize(), nullptr, &VertexShader);
	if (FAILED(hr))
	{
		std::string ErrorMsg = "Failed to create Vertex Shader (HRESULT: " + std::to_string(hr) + ")\n";
		OutputDebugStringA(ErrorMsg.c_str());
		vertexShaderCSO->Release();
		pixelShaderCSO->Release();
		return;
	}

	CachedVertexShaderSize = vertexShaderCSO->GetBufferSize();
	MemoryStats::AddVertexShaderMemory(static_cast<uint32>(CachedVertexShaderSize));

	// Pixel Shader 생성
	hr = InDevice->CreatePixelShader(pixelShaderCSO->GetBufferPointer(), pixelShaderCSO->GetBufferSize(), nullptr, &PixelShader);
	if (FAILED(hr))
	{
		std::string ErrorMsg = "Failed to create Pixel Shader (HRESULT: " + std::to_string(hr) + ")\n";
		OutputDebugStringA(ErrorMsg.c_str());
		Release();
		vertexShaderCSO->Release();
		pixelShaderCSO->Release();
		return;
	}

	CachedPixelShaderSize = pixelShaderCSO->GetBufferSize();
	MemoryStats::AddPixelShaderMemory(static_cast<uint32>(CachedPixelShaderSize));

	// Input Layout 생성 (fullscreen quad 등 vertex buffer 없는 셰이더는 스킵)
	if (InInputElements && InInputElementCount > 0)
	{
		hr = InDevice->CreateInputLayout(InInputElements, InInputElementCount, vertexShaderCSO->GetBufferPointer(), vertexShaderCSO->GetBufferSize(), &InputLayout);
		if (FAILED(hr))
		{
			std::string ErrorMsg = "Failed to create Input Layout (HRESULT: " + std::to_string(hr) + ")\n";
			OutputDebugStringA(ErrorMsg.c_str());
			Release();
			vertexShaderCSO->Release();
			pixelShaderCSO->Release();
			return;
		}
	}

	vertexShaderCSO->Release();
	pixelShaderCSO->Release();
}

void FShader::CreateCompute(ID3D11Device* InDevice, const wchar_t* InFilePath, const char* InCSEntryPoint,
	const D3D_SHADER_MACRO* InDefines)
{
	Release();

	// === 컴퓨트 셰이더 핫 리로드용 데이터 캐싱 추가 ===
	CachedFilePath = InFilePath ? InFilePath : L"";
	CachedCSEntry = InCSEntryPoint ? InCSEntryPoint : "";
	bIsComputeShader = true; // 컴퓨트 셰이더임을 명시

	ID3DBlob* computeShaderCSO = nullptr;
	ID3DBlob* errorBlob = nullptr;

	CachedDefines.clear();
	if (InDefines)
	{
		const D3D_SHADER_MACRO* Macro = InDefines;
		while (Macro->Name != nullptr)
		{
			CachedDefines.push_back(*Macro);
			Macro++;
		}
		CachedDefines.push_back({ nullptr, nullptr });
	}
	bCanHotReload = true;

	HRESULT hr = D3DCompileFromFile(InFilePath, InDefines, D3D_COMPILE_STANDARD_FILE_INCLUDE, InCSEntryPoint, "cs_5_0", 0, 0, &computeShaderCSO, &errorBlob);
	if (FAILED(hr))
	{
		if (errorBlob)
		{
			MessageBoxA(nullptr, (char*)errorBlob->GetBufferPointer(), "Compute Shader Compile Error", MB_OK | MB_ICONERROR);
			errorBlob->Release();
		}
		return;
	}

	hr = InDevice->CreateComputeShader(computeShaderCSO->GetBufferPointer(), computeShaderCSO->GetBufferSize(), nullptr, &ComputeShader);
	if (FAILED(hr))
	{
		std::string ErrorMsg = "Failed to create Compute Shader (HRESULT: " + std::to_string(hr) + ")\n";
		OutputDebugStringA(ErrorMsg.c_str());
		computeShaderCSO->Release();
		return;
	}

	CachedComputeShaderSize = computeShaderCSO->GetBufferSize();
	MemoryStats::AddComputeShaderMemory(static_cast<uint32>(CachedComputeShaderSize));

	computeShaderCSO->Release();
}

void FShader::BindCompute(ID3D11DeviceContext* InDeviceContext) const
{
	if (ComputeShader)
	{
		InDeviceContext->CSSetShader(ComputeShader, nullptr, 0);
	}
}

void FShader::Release()
{
	if (InputLayout)
	{
		InputLayout->Release();
		InputLayout = nullptr;
	}
	if (PixelShader)
	{
		MemoryStats::SubPixelShaderMemory(static_cast<uint32>(CachedPixelShaderSize));
		CachedPixelShaderSize = 0;

		PixelShader->Release();
		PixelShader = nullptr;
	}
	if (VertexShader)
	{
		MemoryStats::SubVertexShaderMemory(static_cast<uint32>(CachedVertexShaderSize));
		CachedVertexShaderSize = 0;

		VertexShader->Release();
		VertexShader = nullptr;
	}
	if (ComputeShader)
	{
		MemoryStats::SubComputeShaderMemory(static_cast<uint32>(CachedComputeShaderSize));
		CachedComputeShaderSize = 0;

		ComputeShader->Release();
		ComputeShader = nullptr;
	}
}

void FShader::Bind(ID3D11DeviceContext* InDeviceContext) const
{
	InDeviceContext->IASetInputLayout(InputLayout);
	InDeviceContext->VSSetShader(VertexShader, nullptr, 0);
	InDeviceContext->PSSetShader(PixelShader, nullptr, 0);
}

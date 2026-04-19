#include "Shader.h"
#include "Profiling/MemoryStats.h"

#include <iostream>
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

	const D3D_SHADER_MACRO* ShaderDefines = InDefines;
	if (!ShaderDefines && InFilePath)
	{
		constexpr std::wstring_view UberLitPath = L"Shaders/UberLit.hlsl";
		if (std::wstring_view(InFilePath) == UberLitPath)
		{
			ShaderDefines = GetLightingModelShaderMacro(GLightingViewMode);
		}
	}

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
		std::cerr << "Failed to create Vertex Shader (HRESULT: " << hr << ")" << std::endl;
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
		std::cerr << "Failed to create Pixel Shader (HRESULT: " << hr << ")" << std::endl;
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
			std::cerr << "Failed to create Input Layout (HRESULT: " << hr << ")" << std::endl;
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

	ID3DBlob* computeShaderCSO = nullptr;
	ID3DBlob* errorBlob = nullptr;

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
		std::cerr << "Failed to create Compute Shader (HRESULT: " << hr << ")" << std::endl;
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

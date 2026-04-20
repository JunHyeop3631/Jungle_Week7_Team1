#pragma once

#include "Materials/MaterialInterface.h"
#include "Math/Vector.h"

class UTexture2D;
class FArchive;

class UMaterialInstance : public UMaterialInterface
{
public:
	DECLARE_CLASS(UMaterialInstance, UMaterialInterface)

	// 부모 머티리얼 (원본 UMaterial일 수도 있고, 또 다른 UMaterialInstance일 수도 있음)
	UMaterialInterface* Parent = nullptr;
	FString ParentPathFileName; // 직렬화 및 로딩 시 부모를 찾기 위한 경로

	// --- 오버라이드 데이터 ---
	bool bOverride_DiffuseColor = false;
	bool bOverride_AmbientColor = false;
	bool bOverride_SpecularColor = false;
	FVector4 OverriddenDiffuseColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	FVector4 OverriddenAmbientColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	FVector4 OverriddenSpecularColor = FVector4(0.0f, 0.0f, 0.0f, 1.0f);

	bool bOverride_DiffuseTexture = false;
	FString OverriddenDiffuseTexturePath;
	UTexture2D* OverriddenDiffuseTexture = nullptr; // 런타임 캐싱 포인터

	bool bOverride_NormalTexture = false;
	FString OverriddenNormalTexturePath;
	UTexture2D* OverriddenNormalTexture = nullptr;

	virtual EMaterialType GetMaterialType() const override { return EMaterialType::Instance; }

	// --- UMaterialInterface 가상 함수 구현 (위임 로직) ---
	virtual UTexture2D* GetDiffuseTexture() const override
	{
		if (bOverride_DiffuseTexture) return OverriddenDiffuseTexture;
		return Parent ? Parent->GetDiffuseTexture() : nullptr;
	}

	virtual FVector4 GetDiffuseColor() const override
	{
		if (bOverride_DiffuseColor) return OverriddenDiffuseColor;
		return Parent ? Parent->GetDiffuseColor() : FVector4(1.0f, 0.0f, 1.0f, 1.0f);
	}
	virtual FVector4 GetAmbientColor() const override
	{
		if (bOverride_AmbientColor) return OverriddenAmbientColor;
		return Parent ? Parent->GetAmbientColor() : FVector4(1.0f, 1.0f	, 1.0f, 1.0f);
	}
	virtual FVector4 GetSpecularColor() const override
	{
		if (bOverride_SpecularColor) return OverriddenSpecularColor;
		return Parent ? Parent->GetSpecularColor() : FVector4(0.0f, 0.0f, 0.0f, 1.0f);
	}

	// 셰이더는 무조건 부모에게 위임 (인스턴스는 셰이더를 가지지 않음) ★
	virtual ID3D11VertexShader* GetVertexShader() const override
	{
		return Parent ? Parent->GetVertexShader() : nullptr;
	}

	virtual ID3D11PixelShader* GetPixelShader() const override
	{
		return Parent ? Parent->GetPixelShader() : nullptr;
	}

	virtual ID3D11InputLayout* GetInputLayout() const override
	{
		return Parent ? Parent->GetInputLayout() : nullptr;
	}

	virtual UTexture2D* GetNormalTexture() const override
	{
		if (bOverride_NormalTexture) return OverriddenNormalTexture;
		return Parent ? Parent->GetNormalTexture() : nullptr;
	}

	// 직렬화
	virtual void Serialize(FArchive& Ar) override;
};
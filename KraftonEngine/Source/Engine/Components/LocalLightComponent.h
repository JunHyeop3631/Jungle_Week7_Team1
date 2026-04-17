#pragma once
#include "LightComponent.h"
class UBillboardComponent;

class ULocalLightComponent : public ULightComponent
{
public:
	DECLARE_CLASS(ULocalLightComponent, ULightComponent)

	float	GetAttenuationRadius() const { return AttenuationRadius; }
	void	SetAttenuationRadius(float NewRadius) { AttenuationRadius = NewRadius; }
	void	SetTintColor();
	void    SetBillboard(UBillboardComponent* InBillboard) { Billboard = InBillboard; }

	// Override
	void	GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void	PostEditProperty(const char* PropertyName) override;
	void	Serialize(FArchive& Ar) override;

private:
	// 감쇠 반경
	float AttenuationRadius = 1.f;
	// 색 변경 캐싱용 빌보드
	UBillboardComponent* Billboard = nullptr;

};


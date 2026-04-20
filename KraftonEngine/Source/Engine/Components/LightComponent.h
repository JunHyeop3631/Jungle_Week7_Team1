#pragma once
#include "LightComponentBase.h"

class FArchive;

class UBillboardComponent;

// 이번 발제에서는 추가할 내용이 거의 없음.
class ULightComponent : public ULightComponentBase
{
public:
	DECLARE_CLASS(ULightComponent, ULightComponentBase)

	void BeginPlay() override;
	void Serialize(FArchive& Ar) override;
	void SetEditorIconBillboard(UBillboardComponent* InBillboard);
	void PostEditProperty(const char* PropertyName) override;

private:
	// 색 변경 캐싱용 빌보드
	UBillboardComponent* EditorIconBillboard = nullptr;
};


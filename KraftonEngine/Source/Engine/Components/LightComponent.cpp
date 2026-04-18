#include "LightComponent.h"
#include "Object/ObjectFactory.h"
#include "BillboardComponent.h"

IMPLEMENT_CLASS(ULightComponent, ULightComponentBase)

void ULightComponent::SetEditorIconBillboard(UBillboardComponent* InBillboard)
{
	EditorIconBillboard = InBillboard;
}

void ULightComponent::PostEditProperty(const char* PropertyName)
{
	ULightComponentBase::PostEditProperty(PropertyName);

	// Icon Billboard 색 갱신
	if (std::strcmp(PropertyName, "LightColor") == 0)
	{
		if (EditorIconBillboard)
		{
			EditorIconBillboard->SetIconTint(LightColor);
		}
	}
}

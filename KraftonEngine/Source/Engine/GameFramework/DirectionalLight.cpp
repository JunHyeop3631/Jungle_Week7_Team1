#include "DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/BillboardComponent.h"

ADirectionalLight::ADirectionalLight()
{
	DirectionalLight = AddComponent<UDirectionalLightComponent>();
	SetRootComponent(DirectionalLight);

	SpriteComponent = AddComponent<UBillboardComponent>();
	SpriteComponent->AttachToComponent(DirectionalLight);
	SpriteComponent->SetTexture(FName("DirectionalLightIcon"));
	DirectionalLight->SetEditorIconBillboard(SpriteComponent);
}

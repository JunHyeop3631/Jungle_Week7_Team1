#include "SpotLight.h"
#include "Object/Object.h"
#include "Components/SpotLightComponent.h"
#include "Components/BillboardComponent.h"

IMPLEMENT_CLASS(ASpotLight, AActor)

ASpotLight::ASpotLight()
{
	SpotLight = AddComponent<USpotLightComponent>();
	SetRootComponent(SpotLight);

	SpriteComponent = AddComponent<UBillboardComponent>();
	SpriteComponent->AttachToComponent(SpotLight);
	SpriteComponent->SetTexture(FName("SpotLightIcon"));
	SpotLight->SetEditorIconBillboard(SpriteComponent);
}



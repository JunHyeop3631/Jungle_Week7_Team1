#include "AmbientLight.h"
#include "Components/AmbientLightComponent.h"
#include "Components/BillboardComponent.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(AAmbientLight, AActor)

AAmbientLight::AAmbientLight()
{
	AmbientLight = AddComponent<UAmbientLightComponent>();
	SetRootComponent(AmbientLight);

	SpriteComponent = AddComponent<UBillboardComponent>();
	SpriteComponent->AttachToComponent(AmbientLight);
	SpriteComponent->SetTexture(FName("AmbientLightIcon"));
	AmbientLight->SetEditorIconBillboard(SpriteComponent);
}

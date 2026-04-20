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

void ASpotLight::BeginPlay()
{
	AActor::BeginPlay();

	for (UActorComponent* Component : GetComponents())
	{
		if (UBillboardComponent* Billboard = Cast<UBillboardComponent>(Component))
		{
			Billboard->SetVisibility(false);
		}
	}
}

void ASpotLight::Serialize(FArchive& Ar)
{
	AActor::Serialize(Ar);
}


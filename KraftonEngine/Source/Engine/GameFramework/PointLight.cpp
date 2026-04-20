#include "PointLight.h"
#include "Object/ObjectFactory.h"
#include "Components/PointLightComponent.h"
#include "Components/BillboardComponent.h"

IMPLEMENT_CLASS(APointLight, AActor)

APointLight::APointLight()
{
	PointLight = AddComponent<UPointLightComponent>();
	SetRootComponent(PointLight);
	/*PointLight->SetIntensity(3.0f);
	PointLight->SetAttenuationRadius(5.0f);*/
	SpriteComponent = AddComponent<UBillboardComponent>();
	SpriteComponent->AttachToComponent(PointLight);
	SpriteComponent->SetTexture(FName("PointLightIcon"));
	PointLight->SetEditorIconBillboard(SpriteComponent);
}

void APointLight::BeginPlay()
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

void APointLight::Serialize(FArchive& Ar)
{
	AActor::Serialize(Ar);
}

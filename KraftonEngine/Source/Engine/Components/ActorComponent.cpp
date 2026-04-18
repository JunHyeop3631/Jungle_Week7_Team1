#include "ActorComponent.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"
#include "GameFramework/AActor.h"

IMPLEMENT_CLASS(UActorComponent, UObject)

UActorComponent::UActorComponent()
{
	PrimaryComponentTick.Target = this;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;	
	PrimaryComponentTick.bTickEnabled = true;
	PrimaryComponentTick.bTickInEditor = false;
	PrimaryComponentTick.RegisterTickFunction();
}

void UActorComponent::BeginPlay()
{
	if (bAutoActivate)
	{
		Activate();
	}
}

void UActorComponent::Activate()
{
	bIsActive = true;
	PrimaryComponentTick.SetTickEnabled(true);
}

void UActorComponent::Deactivate()
{
	bIsActive = false;
	PrimaryComponentTick.SetTickEnabled(false);
}


UWorld* UActorComponent::GetWorld() const
{
	return Owner ? Owner->GetWorld() : nullptr;
}

void UActorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
}

void UActorComponent::SetActive(bool bNewActive)
{
	if (bNewActive == bIsActive)
	{
		return;
	}

	bIsActive = bNewActive;

	if (bIsActive)
	{
		Activate();
	}
	else
	{
		Deactivate();
	}
}

void UActorComponent::Serialize(FArchive& Ar)
{
	UObject::Serialize(Ar);
	Ar << PrimaryComponentTick.bTickEnabled;
	Ar << PrimaryComponentTick.bTickInEditor;
}

void UActorComponent::SetOwner(AActor* Actor)
{
	Owner = Actor;
}

void UActorComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	//OutProps.push_back({ "Active", EPropertyType::Bool, &bIsActive });
	//OutProps.push_back({ "Auto Activate", EPropertyType::Bool, &bAutoActivate });
	//OutProps.push_back({ "Can Ever Tick", EPropertyType::Bool, &bCanEverTick });
	OutProps.push_back({ "bTickEnable", EPropertyType::Bool, &PrimaryComponentTick.bTickEnabled });
}

void UActorComponent::PostEditProperty(const char* PropertyName)
{
}

void UActorComponent::CollectEditorVisualizations(FRenderBus& RenderBus) const
{
	(void)RenderBus;
}

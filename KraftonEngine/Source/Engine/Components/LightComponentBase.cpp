#include "LightComponentBase.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Render/Proxy/DirtyFlag.h"

IMPLEMENT_CLASS(ULightComponentBase, USceneComponent)

void ULightComponentBase::MarkRenderVisibilityDirty()
{
	MarkProxyDirty(EDirtyFlag::Visibility);

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor) return;
	UWorld* World = OwnerActor->GetWorld();
	if (!World) return;

	// 가시성 변화는 Octree 포함 여부도 좌우하므로 액터 dirty로 반영한다.
	World->UpdateActorInOctree(OwnerActor);
	World->MarkWorldPrimitivePickingBVHDirty();
	World->InvalidateVisibleSet();
}

void ULightComponentBase::MarkProxyDirty(EDirtyFlag flag) const
{
	// if (!SceneProxy || !Owner || !Owner->GetWorld()) return;
	// Owner->GetWorld()->GetScene().MarkProxyDirty(SceneProxy, Flag);
}

void ULightComponentBase::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	USceneComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Intensity", EPropertyType::Float, &Intensity, 0.f, 20.f });
	OutProps.push_back({ "LightColor", EPropertyType::Vec4, &LightColor });
	OutProps.push_back({ "Visible" , EPropertyType::Bool, &bVisible });
}

void ULightComponentBase::PostEditProperty(const char* PropertyName)
{
	USceneComponent::PostEditProperty(PropertyName);
	if (strcmp(PropertyName, "Visible") == 0)
	{
		// Property Editor가 bIsVisible을 직접 수정한 경우 dirty 시퀀스만 전파한다.
		MarkRenderVisibilityDirty();
	}
}

void ULightComponentBase::Serialize(FArchive& Ar)
{
	USceneComponent::Serialize(Ar);

	Ar << bVisible;
	Ar << Intensity;
	Ar << LightColor;
}



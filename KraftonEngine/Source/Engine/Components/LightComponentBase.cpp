#include "LightComponentBase.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Render/Proxy/DirtyFlag.h"
#include "Render/Proxy/LightSceneProxy.h"

IMPLEMENT_CLASS(ULightComponentBase, USceneComponent)

void ULightComponentBase::CreateRenderState()
{
	if (LightProxy) return; // 이미 등록됨

	if (!Owner || !Owner->GetWorld()) return;
	FScene& Scene = Owner->GetWorld()->GetScene();
	LightProxy = Scene.AddLight(this);
}
	

void ULightComponentBase::DestroyRenderState()
{
	if (LightProxy && Owner && Owner->GetWorld())
	{
		FScene& Scene = Owner->GetWorld()->GetScene();
		Scene.RemoveLight(LightProxy);
	}

	LightProxy = nullptr;
}

FLightSceneProxy* ULightComponentBase::CreateLightSceneProxy()
{
	return new FLightSceneProxy(this);
}

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
	if (!LightProxy || !Owner || !Owner->GetWorld()) return;
	Owner->GetWorld()->GetScene().MarkLightProxyDirty(LightProxy, flag);
}

void ULightComponentBase::SetVisibility(bool bNewVisible)
{
	if (bVisible == bNewVisible)
	{
		return;
	}

	bVisible = bNewVisible;
	MarkRenderVisibilityDirty();
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
	else if (strcmp(PropertyName, "Intensity") == 0 || strcmp(PropertyName, "LightColor") == 0)
	{
		MarkProxyDirty(EDirtyFlag::LightData);
	}
}

void ULightComponentBase::Serialize(FArchive& Ar)
{
	USceneComponent::Serialize(Ar);

	Ar << bVisible;
	Ar << Intensity;
	Ar << LightColor;
}

void ULightComponentBase::OnTransformDirty()
{
	MarkProxyDirty(EDirtyFlag::Transform);
}


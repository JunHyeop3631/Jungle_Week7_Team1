#include "LightComponentBase.h"

#include "Object/ObjectFactory.h"

#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "GameFramework/World.h"
#include "Render/Proxy/FScene.h"

IMPLEMENT_CLASS(ULightComponentBase, USceneComponent)

IMPLEMENT_CLASS(ULightComponent, ULightComponentBase)

void ULightComponentBase::CreateRenderState()
{
	if (lightData) return; // 이미 등록됨

	// Owner → World → FScene 경로로 접근
	if (!Owner || !Owner->GetWorld()) return;
	FScene& Scene = Owner->GetWorld()->GetScene();
	lightData = Scene.AddLight(this);
}

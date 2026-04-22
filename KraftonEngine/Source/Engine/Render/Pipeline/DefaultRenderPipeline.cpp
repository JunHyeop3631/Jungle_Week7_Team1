#include "DefaultRenderPipeline.h"

#include "Renderer.h"
#include "SceneRenderSetup.h"
#include "Engine/Runtime/Engine.h"
#include "Components/CameraComponent.h"
#include "GameFramework/World.h"

FDefaultRenderPipeline::FDefaultRenderPipeline(UEngine* InEngine, FRenderer& InRenderer)
	: Engine(InEngine)
{
}

FDefaultRenderPipeline::~FDefaultRenderPipeline()
{
}

void FDefaultRenderPipeline::Execute(float DeltaTime, FRenderer& Renderer)
{
	Bus.Clear();

	UWorld* World = Engine->GetWorld();
	if (World)
	{
		World->GetScene().UpdateDirtyLightProxies();
	}

	UCameraComponent* Camera = World ? World->GetActiveCamera() : nullptr;
	if (Camera)
	{
		FShowFlags ShowFlags;
		EViewMode ViewMode = EViewMode::Unlit;

		Bus.SetCameraInfo(Camera);
		Bus.SetRenderSettings(ViewMode, ShowFlags);
		Bus.SetLightingData(World->GetScene().GetLightingData());
		PopulateScenePostProcessConstants(World, Bus);

		Collector.CollectWorld(World, Bus);
		Collector.CollectDebugDraw(World->GetDebugDrawQueue(), Bus);
	}

	Renderer.PrepareBatchers(Bus);
	Renderer.BeginFrame();
	Renderer.Render(Bus);
	Renderer.EndFrame();
}

// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/MyGameInstance.h"
#include "Game/BlackoutExtension.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameViewportClient.h"
#include "TimerManager.h"
#include "Widgets/SWidget.h"
#include "HAL/PlatformTime.h"


// ==============================================================================
// 核心生命周期与组件 (Core Lifecycle & Components)
// ==============================================================================
#pragma region

void UMyGameInstance::Init()
{
	Super::Init();

	BlackoutExt = FSceneViewExtensions::NewExtension<FBlackoutExtension>();

	FWorldDelegates::OnSeamlessTravelTransition.AddUObject(this, &UMyGameInstance::HandleStartTravel);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UMyGameInstance::HandleEndTravel);

	BeginStreamingPauseDelegate.BindUObject(this, &UMyGameInstance::OnBeginStreamingPause);
	EndStreamingPauseDelegate.BindUObject(this, &UMyGameInstance::OnEndStreamingPause);

	if (GEngine)
	{
		GEngine->RegisterBeginStreamingPauseRenderingDelegate(&BeginStreamingPauseDelegate);
		GEngine->RegisterEndStreamingPauseRenderingDelegate(&EndStreamingPauseDelegate);
	}
}

void UMyGameInstance::Shutdown()
{
	if (GEngine)
	{
		GEngine->RegisterBeginStreamingPauseRenderingDelegate(nullptr);
		GEngine->RegisterEndStreamingPauseRenderingDelegate(nullptr);
	}

	FWorldDelegates::OnSeamlessTravelTransition.RemoveAll(this);
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);

	if (BlackoutExt.IsValid())
	{
		BlackoutExt->bIsActive = false;
		BlackoutExt.Reset();
	}

	HideFakeLoadingScreen();
	Super::Shutdown();
}

//所以这两个空函数体内无法写入任何实际有意义的代码。
// 它们目前在你的源码树中，纯粹是为了通过绑定来覆盖并破坏引擎默认拉起“三个点图标”的底层多线程机制。
// 虽然不能删，但必须明确：任何试图在这里做状态清理的逻辑都是绝对失效的。
void UMyGameInstance::OnBeginStreamingPause(FViewport* Viewport)
{
}

void UMyGameInstance::OnEndStreamingPause()
{
}

#pragma endregion


// ==============================================================================
// 伪加载管线 UI 管理 (Fake Loading Pipeline UI)
// ==============================================================================
#pragma region

void UMyGameInstance::HandleStartTravel(UWorld* CurrentWorld)
{
	if (BlackoutExt.IsValid())
	{
		BlackoutExt->bIsActive = true;
	}

	// 漫游开始，重置状态锁
	bEngineIsReady = false;
	bMinTimeElapsed = false;
}

void UMyGameInstance::HandleEndTravel(UWorld* NewWorld)
{
	// 1. 落地新世界第一帧，立刻解除渲染层的 3D 黑场拦截，交棒给 UI
	if (BlackoutExt.IsValid())
	{
		BlackoutExt->bIsActive = false;
	}

	// 2. 尝试拉起动态 UI
	ShowFakeLoadingScreen(nullptr);

	// 3. 标记引擎已经反序列化就绪
	bEngineIsReady = true;

	// 4. 触发一次检查：如果加载太慢（比最小显示时间还长），则直接由这里关闭 UI
	if (bMinTimeElapsed)
	{
		CheckAndHideLoadingScreen();
	}
}

void UMyGameInstance::PlayScreenOffUI(TSoftClassPtr<class UUserWidget> ScreenOffUIClass)
{
	// 如果关卡设计师没配置熄屏UI，我们就直接返回，后面会直接硬切
	if (ScreenOffUIClass.IsNull()) return;

	if (UClass* WidgetClass = ScreenOffUIClass.LoadSynchronous())
	{
		if (UUserWidget* ScreenOffWidget = CreateWidget<UUserWidget>(this, WidgetClass))
		{
			// 加载到极高的层级，挡住一切
			ScreenOffWidget->AddToViewport(10000);

			// 注意：UMG 蓝图里，在 Event Construct 节点直接让它自动播放熄屏动画！
		}
	}
}

void UMyGameInstance::ShowFakeLoadingScreen(TSoftClassPtr<class UUserWidget> CustomUI)
{
	if (PureFakeLoadingSlate.IsValid()) return;

	TSoftClassPtr<UUserWidget> TargetUIClass = CustomUI.IsNull() ? DefaultLoadingUIClass : CustomUI;
	if (UClass* WidgetClass = TargetUIClass.LoadSynchronous())
	{
		if (UUserWidget* LoadingWidget = CreateWidget<UUserWidget>(this, WidgetClass))
		{
			PureFakeLoadingSlate = LoadingWidget->TakeWidget();
			if (PureFakeLoadingSlate.IsValid() && GEngine && GEngine->GameViewport)
			{
				GEngine->GameViewport->AddViewportWidgetContent(PureFakeLoadingSlate.ToSharedRef(), 10001);
			}
		}
	}

	// 记录 UI 真正呈现的绝对时间秒数
	UIStartTime = FPlatformTime::Seconds();

	// 开启一个单次定时器，到时间后宣判“最小显示时间已到”
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(FakeLoadingTimerHandle, [this]()
			{
				bMinTimeElapsed = true;
				// 如果时间到了，且引擎早已就绪，那就直接关掉 UI 迎接玩家
				if (bEngineIsReady)
				{
					CheckAndHideLoadingScreen();
				}
			}, MinUIShowDuration, false);
	}
}

void UMyGameInstance::CheckAndHideLoadingScreen()
{
	// 只有双轨条件全部满足，才执行最终的 UI 清除
	if (bEngineIsReady && bMinTimeElapsed)
	{
		HideFakeLoadingScreen();
	}
}

void UMyGameInstance::HideFakeLoadingScreen()
{
	if (PureFakeLoadingSlate.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(PureFakeLoadingSlate.ToSharedRef());
		PureFakeLoadingSlate.Reset();
	}
	PendingTargetMapName = NAME_None;
}

#pragma endregion
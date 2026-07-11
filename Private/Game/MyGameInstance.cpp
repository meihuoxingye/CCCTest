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

void UMyGameInstance::PlayScreenOffUI(TSoftClassPtr<class UMyTransitionWidgetBase> ScreenOffUIClass)
{
	if (ScreenOffUIClass.IsNull()) return;

	if (UClass* WidgetClass = ScreenOffUIClass.LoadSynchronous())
	{
		// 【神级联动】：因为它是 UMyTransitionWidgetBase，我们只要把它加到视口，
		// 它的 NativeConstruct 就会自动激活体内的动画电池跑入场动画，完全不需要我们手动下令！
		if (UMyTransitionWidgetBase* ScreenOffWidget = CreateWidget<UMyTransitionWidgetBase>(this, WidgetClass))
		{
			ScreenOffWidget->AddToViewport(10000);
			// 注：这个熄屏 UI 会在 ServerTravel 发生时，和旧世界一起自动灰飞烟灭，无需存指针清理
		}
	}
}

void UMyGameInstance::ShowFakeLoadingScreen(TSoftClassPtr<class UMyTransitionWidgetBase> CustomUI)
{
	if (ActiveTransitionUI) return; // 防连按重复拉起

	TSoftClassPtr<UMyTransitionWidgetBase> TargetUIClass = CustomUI.IsNull() ? DefaultLoadingUIClass : CustomUI;
	if (UClass* WidgetClass = TargetUIClass.LoadSynchronous())
	{
		// 【纯净创建】：直接存入保命指针，直接上屏！
		ActiveTransitionUI = CreateWidget<UMyTransitionWidgetBase>(this, WidgetClass);
		if (ActiveTransitionUI)
		{
			ActiveTransitionUI->AddToViewport(10001);
		}
	}

	UIStartTime = FPlatformTime::Seconds();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(FakeLoadingTimerHandle, [this]()
			{
				bMinTimeElapsed = true;
				if (bEngineIsReady) CheckAndHideLoadingScreen();
			}, MinUIShowDuration, false);
	}
}

void UMyGameInstance::CheckAndHideLoadingScreen()
{
	if (bEngineIsReady && bMinTimeElapsed)
	{
		HideFakeLoadingScreen();
	}
}

void UMyGameInstance::HideFakeLoadingScreen()
{
	if (ActiveTransitionUI)
	{
		// 引擎就绪！直接通知 UI，UI 内部的电池会切到退场状态，开始擦除动画！
		ActiveTransitionUI->NotifyEngineReady();
	}
	else
	{
		FinalizeLoadingScreenRemoval();
	}
}

void UMyGameInstance::FinalizeLoadingScreenRemoval()
{
	if (ActiveTransitionUI)
	{
		// 物理移除，释放内存。这行代码只会被 UI 播完动画后自己反向调用！
		ActiveTransitionUI->RemoveFromParent();
		ActiveTransitionUI = nullptr;
	}
	PendingTargetMapName = NAME_None;
}

#pragma endregion
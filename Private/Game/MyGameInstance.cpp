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

	// 崩溃修复：引擎销毁期间，绝对禁止调用 UMG 的动画系统！
	// 直接从内存层面物理抹杀 Widget，不留任何遗言！
	if (ActiveTransitionUI)
	{
		ActiveTransitionUI->RemoveFromParent();
		ActiveTransitionUI = nullptr;
	}

	Super::Shutdown();
}

// 这两个空函数体内无法写入任何实际有意义的代码。
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

	bEngineIsReady = false;
	bMinTimeElapsed = false;
}

void UMyGameInstance::HandleEndTravel(UWorld* NewWorld)
{
	if (BlackoutExt.IsValid())
	{
		BlackoutExt->bIsActive = false;
	}

	// ------------------------------------------------------------------------------
	// 记录一：Persistent Level 落地的确切时间
	// ------------------------------------------------------------------------------
	PersistentLevelLoadTime = FPlatformTime::Seconds();

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, FString::Printf(TEXT("[1] Persistent 进内存时间: %f"), PersistentLevelLoadTime));
	}
	UE_LOG(LogTemp, Warning, TEXT("[TimeTracker] Persistent Level 进内存！绝对时间: %f"), PersistentLevelLoadTime);
	// ------------------------------------------------------------------------------

	ShowFakeLoadingScreen(nullptr);

	// 用 0.05 秒这种极高频率去轮询，抓出时间差的真凶！
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(EngineReadyPollTimerHandle, this, &UMyGameInstance::PollEngineReadyStatus, 0.05f, true);
	}
}

void UMyGameInstance::PollEngineReadyStatus()
{
	UWorld* World = GetWorld();
	if (!World) return;

	// 纯净的引擎基础状态探测，不搞花里胡哨的系统判断
	bool bIsFullyLoaded = World->HasBegunPlay();

	if (bIsFullyLoaded && !World->AreAlwaysLoadedLevelsLoaded())
	{
		bIsFullyLoaded = false;
	}

	if (bIsFullyLoaded)
	{
		World->GetTimerManager().ClearTimer(EngineReadyPollTimerHandle);

		// ------------------------------------------------------------------------------
		// 记录二：引擎真正宣告 Ready 的时间，并算出精确的真实物理时间差！
		// ------------------------------------------------------------------------------
		double EngineReadyTime = FPlatformTime::Seconds();
		double TimeDelta = EngineReadyTime - PersistentLevelLoadTime;

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red, FString::Printf(TEXT("[2] 引擎 Ready 时间: %f"), EngineReadyTime));
			GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Cyan, FString::Printf(TEXT(">>> 真实加载耗时差 (2 - 1): %f 秒 <<<"), TimeDelta));
		}
		UE_LOG(LogTemp, Error, TEXT("[TimeTracker] 引擎宣告 Ready！绝对时间: %f | 距离落地耗时: %f 秒"), EngineReadyTime, TimeDelta);
		// ------------------------------------------------------------------------------

		bEngineIsReady = true;

		if (bMinTimeElapsed)
		{
			CheckAndHideLoadingScreen();
		}
	}
}

FMapTransitionConfig UMyGameInstance::GetMapTransitionConfig(FName MapName) const
{
	if (const FMapTransitionConfig* FoundConfig = MapTransitionRegistry.Find(MapName))
	{
		return *FoundConfig;
	}
	return DefaultTransitionConfig;
}

void UMyGameInstance::PlayScreenOffUI(TSoftClassPtr<class UMyTransitionWidgetBase> ScreenOffUIClass, float InDuration)
{
	if (ScreenOffUIClass.IsNull()) return;

	if (UClass* WidgetClass = ScreenOffUIClass.LoadSynchronous())
	{
		if (UMyTransitionWidgetBase* ScreenOffWidget = CreateWidget<UMyTransitionWidgetBase>(this, WidgetClass))
		{
			ScreenOffWidget->SetTransitionDuration(InDuration);
			ScreenOffWidget->AddToViewport(10000);
		}
	}
}

void UMyGameInstance::ShowFakeLoadingScreen(TSoftClassPtr<class UMyTransitionWidgetBase> CustomUI)
{
	if (ActiveTransitionUI) return;

	FMapTransitionConfig Config = GetMapTransitionConfig(PendingTargetMapName);
	TSoftClassPtr<UMyTransitionWidgetBase> TargetUIClass = CustomUI.IsNull() ? Config.LoadingScreenUIClass : CustomUI;
	float ActualDuration = Config.MinLoadingTime;

	if (UClass* WidgetClass = TargetUIClass.LoadSynchronous())
	{
		ActiveTransitionUI = CreateWidget<UMyTransitionWidgetBase>(this, WidgetClass);
		if (ActiveTransitionUI)
		{
			ActiveTransitionUI->SetLoadingTimeConfig(Config.MinLoadingTime, Config.HoldTimeAtFull);
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
			}, ActualDuration, false);
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
		ActiveTransitionUI->RemoveFromParent();
		ActiveTransitionUI = nullptr;
	}
	PendingTargetMapName = NAME_None;
}

#pragma endregion
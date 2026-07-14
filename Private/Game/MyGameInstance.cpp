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

#include "HAL/IConsoleManager.h" // 【新增】：用于直接操控底层控制台变量

// ==============================================================================
// 核心生命周期与组件 (Core Lifecycle & Components)
// ==============================================================================
#pragma region

void UMyGameInstance::Init()
{
	Super::Init();

	// =====================================================================
	// 【强行解锁编辑器 PIE 无缝漫游】
	// 直接在内存级覆写引擎底层 CVar，不碰任何 ini 配置文件！
	// =====================================================================
#if WITH_EDITOR
	if (IConsoleVariable* PIESeamlessCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("net.AllowPIESeamlessTravel")))
	{
		PIESeamlessCVar->Set(1, ECVF_SetByCode);
	}
#endif


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

	// 【新增防线：防止被打断导致的僵尸UI内存泄漏】
	// 如果漫游发生时居然还有 UI 活着，立刻物理处决，确保新旧交替绝对纯净！
	if (ActiveTransitionUI)
	{
		ActiveTransitionUI->RemoveFromParent();
		ActiveTransitionUI = nullptr;
	}

	// 漫游开始，重置状态锁，包含新增的互斥锁
	bEngineIsReady = false;
	bMinTimeElapsed = false;
	bIsHiding = false;
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

	// Persistent Level 进内存，拉起 UI。
	// 此时引擎流送还没完 (bEngineIsReady 默认为 false)，UI 老老实实走 15% 盲开。
	ShowFakeLoadingScreen(nullptr);

	// 自动化接管：开启高频雷达，每 0.05 秒轮询引擎底层流送状态
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
		// 引擎就绪，关掉探测雷达
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

		// 宣布就绪！UI 此时接收到信号，瞬间触发平滑提速
		bEngineIsReady = true;

		// 如果字典时间已经到了，强制关门
		if (bMinTimeElapsed)
		{
			CheckAndHideLoadingScreen();
		}
	}
}

FMapTransitionConfig UMyGameInstance::GetMapTransitionConfig(FName MapName) const
{
	// O(1) 极速哈希查表，查到了就返回专属配置
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
		// 神级联动：因为它是 UMyTransitionWidgetBase，我们只要把它加到视口，
		// 它的 NativeConstruct 就会自动激活体内的动画电池跑入场动画，完全不需要我们手动下令！
		if (UMyTransitionWidgetBase* ScreenOffWidget = CreateWidget<UMyTransitionWidgetBase>(this, WidgetClass))
		{
			// 铁律执行：在上屏（NativeConstruct）触发动画前，强行将 LD 的时间注入电池！
			ScreenOffWidget->SetTransitionDuration(InDuration);

			ScreenOffWidget->AddToViewport(10000);
			// 注：这个熄屏 UI 会在 ServerTravel 发生时，和旧世界一起自动灰飞烟灭，无需存指针清理
		}
	}
}

void UMyGameInstance::ShowFakeLoadingScreen(TSoftClassPtr<class UMyTransitionWidgetBase> CustomUI)
{
	if (ActiveTransitionUI) return;

	// 核心魔法：大管家自己用 PendingTargetMapName 查字典！
	FMapTransitionConfig Config = GetMapTransitionConfig(PendingTargetMapName);

	// 优先用外部强传的 CustomUI，如果没有就用字典里的
	TSoftClassPtr<UMyTransitionWidgetBase> TargetUIClass = CustomUI.IsNull() ? Config.LoadingScreenUIClass : CustomUI;
	float ActualDuration = Config.MinLoadingTime;

	if (UClass* WidgetClass = TargetUIClass.LoadSynchronous())
	{
		ActiveTransitionUI = CreateWidget<UMyTransitionWidgetBase>(this, WidgetClass);
		if (ActiveTransitionUI)
		{
			// 在 UI 上屏前，把字典里的时间契约强行压入 UI 的状态机中
			ActiveTransitionUI->SetLoadingTimeConfig(Config.MinLoadingTime, Config.HoldTimeAtFull);
			ActiveTransitionUI->AddToViewport(10001);
		}
	}

	UIStartTime = FPlatformTime::Seconds();

	if (UWorld* World = GetWorld())
	{
		// 【新增防线：UE C++ 弱指针保护】
		// 严防底层垃圾回收机制造成的崩溃，将 this 封印进 TWeakObjectPtr
		TWeakObjectPtr<UMyGameInstance> WeakThis(this);

		// 大管家的最高主宰倒计时：时间一到，无论 UI 跑没跑完，强制触发隐藏逻辑
		World->GetTimerManager().SetTimer(FakeLoadingTimerHandle, [WeakThis]()
			{
				if (UMyGameInstance* StrongThis = WeakThis.Get())
				{
					StrongThis->bMinTimeElapsed = true;
					if (StrongThis->bEngineIsReady) StrongThis->CheckAndHideLoadingScreen();
				}
			}, ActualDuration, false);
	}
}

void UMyGameInstance::CheckAndHideLoadingScreen()
{
	// 【新增防线：互斥锁保护】
	// 拦截极端情况下的高频冗余触发，退场指令有且只有一次生效的机会！
	if (bEngineIsReady && bMinTimeElapsed && !bIsHiding)
	{
		bIsHiding = true;
		HideFakeLoadingScreen();
	}
}

void UMyGameInstance::HideFakeLoadingScreen()
{
	if (ActiveTransitionUI)
	{
		// 引擎就绪且倒计时已到！直接通知 UI，UI 内部的电池会切到退场状态，开始擦除动画！
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
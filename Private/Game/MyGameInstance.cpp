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
	// 调用父类的原生初始化逻辑
	Super::Init();

#if WITH_EDITOR
	// 强行解锁编辑器 PIE 无缝漫游：直接在控制台管理器中查找控制台变量指针
	if (IConsoleVariable* PIESeamlessCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("net.AllowPIESeamlessTravel")))
	{
		// 内存级强行覆写变量值为 1，绕过 ini 配置文件限制，逼迫编辑器在 PIE 下也必须开启无缝漫游机制
		PIESeamlessCVar->Set(1, ECVF_SetByCode);
	}
#endif

	// 初始化黑幕场景视图扩展，用于在渲染依赖图末端拦截残影和漏光
	BlackoutExt = FSceneViewExtensions::NewExtension<FBlackoutExtension>();

	// 注册全局无缝漫游开始钩子，当旧世界准备好断连并起航时触发
	FWorldDelegates::OnSeamlessTravelTransition.AddUObject(this, &UMyGameInstance::HandleStartTravel);

	// 注册全局关卡加载完毕钩子，当新世界反序列化完毕且进内存后第一时间触发
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UMyGameInstance::HandleEndTravel);

	// 将本地的空回调函数绑定到引擎流送渲染暂停委托上
	BeginStreamingPauseDelegate.BindUObject(this, &UMyGameInstance::OnBeginStreamingPause);
	EndStreamingPauseDelegate.BindUObject(this, &UMyGameInstance::OnEndStreamingPause);

	if (GEngine)
	{
		// 强行将自己绑定的空委托物理注入并挂载到引擎底层
		GEngine->RegisterBeginStreamingPauseRenderingDelegate(&BeginStreamingPauseDelegate);

		// 破坏并覆盖掉引擎默认拉起“三个点图标”的底层黑屏流送挂起机制，夺回 UI 渲染控制权
		GEngine->RegisterEndStreamingPauseRenderingDelegate(&EndStreamingPauseDelegate);
	}
}

void UMyGameInstance::Shutdown()
{
	if (GEngine)
	{
		// 引擎关闭时，安全注销并解绑底层的流送暂停渲染委托
		GEngine->RegisterBeginStreamingPauseRenderingDelegate(nullptr);
		GEngine->RegisterEndStreamingPauseRenderingDelegate(nullptr);
	}

	// 注销所有的无缝漫游多播委托，防止大管家被销毁后产生悬空指针回调
	FWorldDelegates::OnSeamlessTravelTransition.RemoveAll(this);
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);

	if (BlackoutExt.IsValid())
	{
		// 关闭黑幕扩展的状态开关
		BlackoutExt->bIsActive = false;

		// 物理释放场景视图扩展的智能指针，协助引擎清理渲染线程
		BlackoutExt.Reset();
	}

	if (ActiveTransitionUI)
	{
		// 崩溃修复：引擎已进入销毁时序，绝对禁止调用 UMG 的延迟动画系统
		ActiveTransitionUI->RemoveFromParent();

		// 直接从内存层面物理抹杀 Widget 控件，强制置空切断引用链，不留任何触发退场动画的遗言
		ActiveTransitionUI = nullptr;
	}

	// 归还控制权，执行父类的原生销毁
	Super::Shutdown();
}

void UMyGameInstance::OnBeginStreamingPause(FViewport* Viewport)
{
	// 留空：此函数体内无法且严禁写入任何实际状态清理逻辑
}

void UMyGameInstance::OnEndStreamingPause()
{
	// 留空：纯粹为了卡住多线程渲染钩子，任何试图在这里编写表现层的逻辑都是绝对失效的
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
		// 漫游起航瞬间，立刻激活渲染断路器，强制接管并纯黑覆盖后续所有帧的画面
		BlackoutExt->bIsActive = true;
	}

	if (ActiveTransitionUI)
	{
		// 新增防线：防止被打断导致的僵尸UI内存泄漏
		// 如果漫游发生时居然还有 UI 活着，立刻物理处决，确保新旧交替绝对纯净
		ActiveTransitionUI->RemoveFromParent();
		ActiveTransitionUI = nullptr;
	}

	// 漫游开始，彻底重置所有状态锁与互斥锁，为新一轮加载做纯净准备
	bEngineIsReady = false;
	bMinTimeElapsed = false;
	bIsHiding = false;
}

void UMyGameInstance::HandleEndTravel(UWorld* NewWorld)
{
	if (BlackoutExt.IsValid())
	{
		// 新世界已在内存中就绪，解除底层渲染断路器，将画面主导权交还给即将上屏的加载 UI
		BlackoutExt->bIsActive = false;
	}

	// 记录 Persistent Level 落地的确切物理时间戳，作为整个加载计时的绝对零点
	PersistentLevelLoadTime = FPlatformTime::Seconds();

#if !UE_BUILD_SHIPPING
	if (GEngine)
	{
		// 在屏幕上打印 Persistent 关卡进内存的时间戳（打包发行版将自动抹除本段开销）
		GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, FString::Printf(TEXT("[1] Persistent 进内存时间: %f"), PersistentLevelLoadTime));
	}
	// 在后台日志中输出 Persistent 关卡进内存的时间戳
	UE_LOG(LogTemp, Warning, TEXT("[TimeTracker] Persistent Level 进内存！绝对时间: %f"), PersistentLevelLoadTime);
#endif

	// Persistent Level 进内存后立刻拉起 UI 遮罩
	// 此时引擎流送还没完 (bEngineIsReady 默认为 false)，UI 老老实实走 15% 盲开步进
	ShowFakeLoadingScreen(nullptr);

	if (UWorld* World = GetWorld())
	{
		// 自动化接管：开启高频雷达，每 0.05 秒轮询拷问引擎底层流送状态
		World->GetTimerManager().SetTimer(EngineReadyPollTimerHandle, this, &UMyGameInstance::PollEngineReadyStatus, 0.05f, true);
	}
}

void UMyGameInstance::PollEngineReadyStatus()
{
	UWorld* World = GetWorld();
	if (!World) return;

	// 纯净的引擎基础状态探测：查验新世界是否已经开始运行
	bool bIsFullyLoaded = World->HasBegunPlay();

	if (bIsFullyLoaded && !World->AreAlwaysLoadedLevelsLoaded())
	{
		// 二重查验：如果世界已开始运行，但强制常驻的流送关卡还没进内存，立刻否决就绪状态
		bIsFullyLoaded = false;
	}

	if (bIsFullyLoaded)
	{
		// 引擎底层彻底就绪，立刻关掉高频探测雷达以释放 CPU 性能
		World->GetTimerManager().ClearTimer(EngineReadyPollTimerHandle);

		// 记录引擎真正宣告 Ready 的时间戳
		double EngineReadyTime = FPlatformTime::Seconds();

		// 算出从 Persistent 进内存到格子流送完毕的精确物理耗时差
		double TimeDelta = EngineReadyTime - PersistentLevelLoadTime;

#if !UE_BUILD_SHIPPING
		if (GEngine)
		{
			// 将耗时数据打印到屏幕，供开发者评估地图性能（发行版自动抹除）
			GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red, FString::Printf(TEXT("[2] 引擎 Ready 时间: %f"), EngineReadyTime));
			GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Cyan, FString::Printf(TEXT(">>> 真实加载耗时差 (2 - 1): %f 秒 <<<"), TimeDelta));
		}
		// 将耗时数据输出到后台日志
		UE_LOG(LogTemp, Error, TEXT("[TimeTracker] 引擎宣告 Ready！绝对时间: %f | 距离落地耗时: %f 秒"), EngineReadyTime, TimeDelta);
#endif

		// 宣布就绪！UI 此时接收到该状态变动信号，瞬间触发平滑提速变轨逻辑
		bEngineIsReady = true;

		if (bMinTimeElapsed)
		{
			// 如果大管家定下的最低加载时间已经到了，立刻强制关门触发 UI 退场
			CheckAndHideLoadingScreen();
		}
	}
}

FMapTransitionConfig UMyGameInstance::GetMapTransitionConfig(FName MapName) const
{
	if (const FMapTransitionConfig* FoundConfig = MapTransitionRegistry.Find(MapName))
	{
		// O(1) 极速哈希查表，查到了就直接返回该地图专属的转场配置
		return *FoundConfig;
	}

	// 如果配置字典中没有该地图，安全降级，返回通用的默认配置
	return DefaultTransitionConfig;
}

void UMyGameInstance::PlayScreenOffUI(TSoftClassPtr<class UMyTransitionWidgetBase> ScreenOffUIClass, float InDuration)
{
	if (ScreenOffUIClass.IsNull()) return;

	if (UClass* WidgetClass = ScreenOffUIClass.LoadSynchronous())
	{
		if (UMyTransitionWidgetBase* ScreenOffWidget = CreateWidget<UMyTransitionWidgetBase>(this, WidgetClass))
		{
			// 铁律执行：在上屏（NativeConstruct）触发动画前，强行将设计师规定的时间注入 UI 内部电池
			ScreenOffWidget->SetTransitionDuration(InDuration);

			// 赋予极高的 ZOrder，确保黑幕能遮挡游戏内的任何层级
			// 注：这个熄屏 UI 会在 ServerTravel 发生时，随旧世界一起自动灰飞烟灭，无需保留指针清理
			ScreenOffWidget->AddToViewport(10000);
		}
	}
}

void UMyGameInstance::ShowFakeLoadingScreen(TSoftClassPtr<class UMyTransitionWidgetBase> CustomUI)
{
	if (ActiveTransitionUI) return;

	// 核心魔法：大管家自己用 PendingTargetMapName 去查字典提取目标配置
	FMapTransitionConfig Config = GetMapTransitionConfig(PendingTargetMapName);

	// 优先使用外部强制传入的 CustomUI 软指针，如果外部没传，就使用字典里查到的默认加载界面
	TSoftClassPtr<UMyTransitionWidgetBase> TargetUIClass = CustomUI.IsNull() ? Config.LoadingScreenUIClass : CustomUI;

	// 提取设计师在字典中配置的目标最短等待时间
	float ActualDuration = Config.MinLoadingTime;

	if (UClass* WidgetClass = TargetUIClass.LoadSynchronous())
	{
		ActiveTransitionUI = CreateWidget<UMyTransitionWidgetBase>(this, WidgetClass);
		if (ActiveTransitionUI)
		{
			// 在 UI 上屏前，把字典里的时间契约强行压入 UI 的状态机中，接管其动画生命周期
			ActiveTransitionUI->SetLoadingTimeConfig(Config.MinLoadingTime, Config.HoldTimeAtFull);

			// 将加载界面加到视口，ZOrder 设为极高的 10001
			ActiveTransitionUI->AddToViewport(10001);
		}
	}

	// 记录 UI 正式开始播放动画的绝对时间
	UIStartTime = FPlatformTime::Seconds();

	if (UWorld* World = GetWorld())
	{
		// 新增防线：UE C++ 弱指针保护
		// 严防底层垃圾回收机制造成的崩溃，将 this 封印进 TWeakObjectPtr
		TWeakObjectPtr<UMyGameInstance> WeakThis(this);

		// 大管家的最高主宰倒计时：时间一到，无论 UI 跑没跑完，强制触发隐藏逻辑
		World->GetTimerManager().SetTimer(FakeLoadingTimerHandle, [WeakThis]()
			{
				if (UMyGameInstance* StrongThis = WeakThis.Get())
				{
					// 时间契约到期，标记最少等待时间已过
					StrongThis->bMinTimeElapsed = true;

					// 如果此时引擎也已经 Ready，立刻执行关门（退场）操作
					if (StrongThis->bEngineIsReady) StrongThis->CheckAndHideLoadingScreen();
				}
			}, ActualDuration, false);
	}
}

void UMyGameInstance::CheckAndHideLoadingScreen()
{
	if (bEngineIsReady && bMinTimeElapsed && !bIsHiding)
	{
		// 新增防线：互斥锁保护
		// 拦截极端情况下的高频冗余触发，退场指令有且只有一次生效的机会！
		bIsHiding = true;

		// 满足所有前置条件，正式执行 UI 隐藏
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
		// 兜底逻辑：如果 UI 因不可抗力丢失，直接完成转场收尾工作
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

	// 【新增】：一次大一统漫游彻底闭环，销毁跨界车票，防玩家死后重生依然触发幽灵传送！
	PendingTravelRoute = nullptr;
}

#pragma endregion
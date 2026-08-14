// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/MyGameInstance.h"
#include "Game/BlackoutExtension.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "HAL/PlatformTime.h"

#include "HAL/IConsoleManager.h" // 【新增】：用于直接操控底层控制台变量
#include "MapTravel/MyMapTravelSubsystem.h"
#include "Kismet/GameplayStatics.h"

#include "UI/Transition/MyScreenOffWidget.h"
#include "UI/Transition/MyLoadingScreenWidget.h"

#include "Game/MyGameModeBase.h"
#include "Character/TopCharacter.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "EngineUtils.h"

#include "MapTravel/NetComponent/MyMapTravelNetComponent.h"

#include "World/MyMapAttributeDataAsset.h"


// ==============================================================================
// 核心生命周期与组件 (Core Lifecycle & Components)
// ==============================================================================
#pragma region

void UMyGameInstance::Init()
{
	// 调用父类的原生初始化逻辑
	Super::Init();

	// 【极致加固】：无论何种方式启动/重启游戏，彻底清空上一局可能残留的哈希表记忆，防止脏数据污染新战局！
	PlayerClassMemory.Empty();

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

	// ==============================================================================
	// 【终极解药】：填补非无缝（硬跳转）的物理真空！
	// 房主建房（带 ?listen）或客机飞线连接时，引擎必定执行非无缝的 Hard Travel。
	// 这时 OnSeamlessTravelTransition 绝对不会触发！必须监听引擎底层的 PreLoadMap，
	// 在旧世界被毁灭的前一刻，强行呼叫大管家重置 bIsHiding 等转场锁，否则必定死锁！
	// ==============================================================================
	FCoreUObjectDelegates::PreLoadMap.AddLambda([this](const FString& MapName)
		{
			if (UWorld* World = GetWorld())
			{
				// 引擎级别的容错：只要不是在正常的无缝漫游中，就代表是硬跳转
				// 立刻复用无缝的清洗逻辑，将脏数据彻底洗净！
				if (!World->IsInSeamlessTravel())
				{
					HandleStartTravel(World);
				}
			}
		});

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

	if (ActiveLoadingScreenUI)
	{
		// 崩溃修复：引擎已进入销毁时序，绝对禁止调用 UMG 的延迟动画系统
		ActiveLoadingScreenUI->RemoveFromParent();

		// 直接从内存层面物理抹杀 Widget 控件，强制置空切断引用链，不留任何触发退场动画的遗言
		ActiveLoadingScreenUI = nullptr;
	}

	// =========================================================================
	// 【新增核心修复】：跨界前物理处决旧世界的黑幕 UI！
	// 绝对禁止大管家把旧世界的 UI 强引用带到新世界，防范 GC 扫描时爆出 
	// "Object from PIE level still referenced" 的致命闪退！
	// =========================================================================
	if (ActiveScreenOffUI)
	{
		ActiveScreenOffUI->RemoveFromParent();
		ActiveScreenOffUI = nullptr;
	}

	// 归还控制权，执行父类的原生销毁
	Super::Shutdown();
}

#pragma endregion


// ==============================================================================
// 跨地图核心记忆库 (Cross-Map Memory)
// ==============================================================================
#pragma region

EMapPhaseType UMyGameInstance::GetMapPhase(const FString& MapShortName) const
{
	// 遍历大管家手中捏着的所有独立地图数据资产
	for (const TObjectPtr<UMyMapAttributeDataAsset>& Asset : MapAttributeAssets)
	{
		// 强校验：确保资产指针有效，且内部的软引用也有效，然后比对短名
		if (Asset && !Asset->MapAsset.IsNull() && Asset->MapAsset.GetAssetName() == MapShortName)
		{
			return Asset->PhaseType;
		}
	}

	// 兜底防线：如果策划忘了给某张图配专属资产，或者忘了加进大名单，一律视为纯动态图！
	return EMapPhaseType::AlwaysDynamic;
}

#pragma endregion


// ==============================================================================
// 伪加载管线 UI 管理 (Fake Loading Pipeline UI)
// ==============================================================================
#pragma region

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

void UMyGameInstance::PlayScreenOffPhaseUI(TSoftClassPtr<class UMyScreenOffWidget> ScreenOffUIClass, float InDuration)
{
	if (ScreenOffUIClass.IsNull()) return;

	if (UClass* WidgetClass = ScreenOffUIClass.LoadSynchronous())
	{
		if (UMyScreenOffWidget* ScreenOffWidget = CreateWidget<UMyScreenOffWidget>(this, WidgetClass))
		{
			// 铁律执行：在上屏（NativeConstruct）触发动画前，强行将设计师规定的时间注入 UI 内部电池
			ScreenOffWidget->SetTransitionDuration(InDuration);

			// 赋予极高的 ZOrder，确保黑幕能遮挡游戏内的任何层级
			// 注：这个熄屏 UI 会在 ServerTravel 发生时，随旧世界一起自动灰飞烟灭，无需保留指针清理
			ScreenOffWidget->AddToViewport(10000);

			// 【核心修复】：必须保存指针！跨地图引擎会随旧世界销毁它，但同地图必须手动销毁！
			ActiveScreenOffUI = ScreenOffWidget;
		}
	}
}

void UMyGameInstance::PlayLoadingPhaseUI(TSoftClassPtr<class UMyLoadingScreenWidget> CustomUI)
{
	if (ActiveLoadingScreenUI) return;

	// 核心魔法：大管家自己用 PendingTargetMapName 去查字典提取目标配置
	FMapTransitionConfig Config = GetMapTransitionConfig(PendingTargetMapName);

	// 优先使用外部强制传入的 CustomUI 软指针，如果外部没传，就使用字典里查到的默认加载界面
	TSoftClassPtr<UMyLoadingScreenWidget> TargetUIClass = CustomUI.IsNull() ? Config.LoadingScreenUIClass : CustomUI;

	// 提取设计师在字典中配置的目标最短等待时间
	float ActualDuration = Config.MinLoadingTime;

	if (UClass* WidgetClass = TargetUIClass.LoadSynchronous())
	{
		ActiveLoadingScreenUI = CreateWidget<UMyLoadingScreenWidget>(this, WidgetClass);
		if (ActiveLoadingScreenUI)
		{
			// 在 UI 上屏前，把字典里的时间契约强行压入 UI 的状态机中，接管其动画生命周期
			ActiveLoadingScreenUI->SetLoadingTimeConfig(Config.MinLoadingTime, Config.HoldTimeAtFull);

			// 将加载界面加到视口，ZOrder 设为极高的 10001
			ActiveLoadingScreenUI->AddToViewport(10001);
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

void UMyGameInstance::HideFakeLoadingScreen()
{
	if (ActiveLoadingScreenUI)
	{
		// 引擎就绪且倒计时已到！直接通知 UI，UI 内部的电池会切到退场状态，开始擦除动画！
		ActiveLoadingScreenUI->NotifyEngineReady();
	}
	else
	{
		// 兜底逻辑：如果 UI 因不可抗力丢失，直接完成转场收尾工作
		FinalizeLoadingScreenRemoval();
	}
}

void UMyGameInstance::FinalizeLoadingScreenRemoval()
{
	// 终极清理防线：检查当前内存中是否还有存活的转场 UI 实例
	if (ActiveLoadingScreenUI)
	{
		// 从视口中物理抹除该 UI，彻底结束它在黑幕期间的屏幕霸权
		ActiveLoadingScreenUI->RemoveFromParent();

		// 强制置空指针，切断强引用链，将其残躯完全移交给虚幻底层的垃圾回收系统 (GC)
		ActiveLoadingScreenUI = nullptr;
	}

	// 状态机重置：清空挂起的目标地图名称缓存，确保下一次传送判定是一张纯净的白纸
	PendingTargetMapName = NAME_None;

	// 【新增】：一次大一统漫游彻底闭环，销毁跨界车票，防玩家死后重生依然触发幽灵传送！
	// (注：此代码目前被注释掉，若后续架构中存在车票生命周期泄漏风险，可随时在此解除封印)
	// PendingTravelRoute = nullptr;

	// 【完美闭环】：无论跨图还是同图，UI 动画播完并销毁后，统一由大管家触发解穴！
	// 安全获取当前世界上下文，防范世界正在被销毁的极端情况
	if (UWorld* World = GetWorld())
	{
		// 跨系统通信：从世界中提取负责处理底层物理与状态流转的传送子系统
		if (UMyMapTravelSubsystem* TravelSub = World->GetSubsystem<UMyMapTravelSubsystem>())
		{
			// 终极解穴令：大管家向子系统下达最终指令，正式恢复玩家的物理碰撞、重力坠落与一切输入控制权！
			TravelSub->RestorePlayerInput();
		}
	}
}

void UMyGameInstance::HandleStartTravel(UWorld* CurrentWorld)
{
	if (BlackoutExt.IsValid())
	{
		// 漫游起航瞬间，立刻激活渲染断路器，强制接管并纯黑覆盖后续所有帧的画面
		BlackoutExt->bIsActive = true;
	}

	if (ActiveLoadingScreenUI)
	{
		// 新增防线：防止被打断导致的僵尸UI内存泄漏
		// 如果漫游发生时居然还有 UI 活着，立刻物理处决，确保新旧交替绝对纯净
		ActiveLoadingScreenUI->RemoveFromParent();
		ActiveLoadingScreenUI = nullptr;
	}

	// =========================================================================
	// 【新增核心修复】：跨界前物理处决旧世界的黑幕 UI！
	// 绝对禁止大管家把旧世界的 UI 强引用带到新世界，防范 GC 扫描时爆出 
	// "Object from PIE level still referenced" 的致命闪退！
	// =========================================================================
	if (ActiveScreenOffUI)
	{
		ActiveScreenOffUI->RemoveFromParent();
		ActiveScreenOffUI = nullptr;
	}

	// 漫游开始，彻底重置所有状态锁与互斥锁，为新一轮加载做纯净准备
	bEngineIsReady = false;
	bMinTimeElapsed = false;
	bIsHiding = false;
}

void UMyGameInstance::HandleEndTravel(UWorld* NewWorld)
{
	if (!NewWorld) return;

	// 1. 【开荒判定】：如果大管家手里没有挂起的车票，说明这是玩家初次启动游戏（开机落地）
	bool bIsFirstBoot = PendingTargetMapName.IsNone();

	// 提取当前真实落地的地图名（剔除 PIE 前缀）
	FString CleanCurrentMap = UGameplayStatics::GetCurrentLevelName(NewWorld, true);
	// 如果是开荒，目标地图名就是当前地图；如果不是，就提取车票上的目标地图名
	FString CleanTargetMap = bIsFirstBoot ? CleanCurrentMap : FPackageName::GetShortName(PendingTargetMapName.ToString());

	// 2. 【核心防线】：精准跳过无缝漫游的过渡层 (TransitionMap)
	// 如果不是开荒，且当前落地的地图根本不是我们要去的地图，说明我们掉进了过渡虚空！
	if (!bIsFirstBoot && !CleanCurrentMap.Contains(CleanTargetMap))
	{
		UE_LOG(LogTemp, Warning, TEXT("⏭️ [表现层] 侦测到无缝过渡地图，跳过 UI 唤醒与黑幕解除: %s"), *CleanCurrentMap);
		return;
	}

	// ==============================================================================
	// 无论是开机开荒，还是真正抵达了新世界，都必须立刻解除底层的渲染断路器！
	// 否则玩家会被永久关在小黑屋里（听声辩位）！
	// ==============================================================================
	if (BlackoutExt.IsValid())
	{
		// 新世界已就绪，交还画面主导权
		BlackoutExt->bIsActive = false;
	}

	// 记录 Persistent Level 落地的确切物理时间戳，作为整个加载计时的绝对零点
	PersistentLevelLoadTime = FPlatformTime::Seconds();

	// 3. 落地 UI 表现
	if (bIsFirstBoot)
	{
		UE_LOG(LogTemp, Warning, TEXT("🚀 [大管家] 侦测到开荒启动，正在加载初代世界: %s"), *CleanCurrentMap);
		// 狸猫换太子：给开荒也塞入当前地图名，以便底层查字典拉起兜底的加载 UI
		PendingTargetMapName = FName(*CleanCurrentMap);
	}

	// Persistent Level 进内存后立刻拉起 UI 遮罩
	// 此时引擎流送还没完，UI 老老实实走盲开步进
	PlayLoadingPhaseUI(nullptr);

	if (UWorld* World = GetWorld())
	{
		// 自动化接管：开启高频雷达，每 0.05 秒轮询拷问引擎底层流送状态
		World->GetTimerManager().SetTimer(EngineReadyPollTimerHandle, this, &UMyGameInstance::PollEngineReadyStatus, 0.05f, true);
	}
}

void UMyGameInstance::ExecuteSameMapTransition(AActor* TeleportingActor, const FTransform& TargetTransform)
{
	// 【核心排毒】：彻底删除了 if (!TeleportingActor) return; 这道旧防线！
	// 因为现在大管家已经被重构成纯粹的表现层（UI）管线，物理逻辑交由网络组件，
	// 所以即使传入 nullptr，也必须坚决拉起黑屏遮罩，绝不中止管线！

	// 1. 初始化转场锁
	// 彻底重置大管家内部的转场状态机，确保上一次的遗留状态不会污染本次同图漫游
	bEngineIsReady = false;
	bMinTimeElapsed = false;
	bIsHiding = false;

	// 安全获取当前世界上下文，防范世界正在被销毁或未初始化的极端情况
	UWorld* World = GetWorld();
	if (!World) return;

	// 2. 剥离 PIE 前缀提取配置
	// 提取当前关卡名，传入 true 代表强行剔除编辑器模式下的 "UEDPIE_0_" 前缀，保证字典哈希查表绝对准确
	FName CurrentMapName = FName(*UGameplayStatics::GetCurrentLevelName(World, true));

	// 通过当前真实的关卡名称，向大管家的全局配置字典请求对应的转场配置信息
	FMapTransitionConfig Config = GetMapTransitionConfig(CurrentMapName);

	// 狸猫换太子：同图传送的本质也是重载状态，将目标地图伪装成当前地图，骗过底层 UI 状态机的核验
	PendingTargetMapName = CurrentMapName;

	// 3. 拉起黑幕遮罩
	// 依据查表得出的配置，拉起专属的熄屏（闭眼）UI，并强行注入设计师规定的淡出时间
	PlayScreenOffPhaseUI(Config.ScreenOffUIClass, Config.ScreenOffDuration);

	// 物理防死锁计算：取系统极限底线 (0.01s)、全局安全延迟与 UI 动画时长的最大值
	// 确保无论策划怎么填表，定时器都拥有充足的物理缓冲时间，绝不穿帮
	float SafeDelay = FMath::Max(0.01f, FMath::Max(SystemSafeDelay, Config.ScreenOffDuration));

	// 内存安全闭环：使用弱指针包装传送实体，防范在这零点几秒的黑屏等待期内，该 Actor 意外死亡或被 GC 销毁造成的野指针崩溃
	TWeakObjectPtr<AActor> WeakActor(TeleportingActor);

	// 4. 等待黑幕完全闭合后，执行下半场
	// 挂起后台高频定时器，让引擎继续跑完这几帧的 UI 渲染
	World->GetTimerManager().SetTimer(SameMapScreenOffTimerHandle, [this, WeakActor, TargetTransform]()
		{
			// 此时屏幕已经达到 100% 绝对纯黑，安全触发物理坐标的瞬间折叠与新数据层的唤醒
			OnSameMapScreenOffFinished(WeakActor, TargetTransform);

		}, SafeDelay, false); // false 代表该闭包回调仅执行一次，绝不循环
}

void UMyGameInstance::OnBeginStreamingPause(FViewport* Viewport)
{
	// 留空：此函数体内无法且严禁写入任何实际状态清理逻辑
}

void UMyGameInstance::OnEndStreamingPause()
{
	// 留空：纯粹为了卡住多线程渲染钩子，任何试图在这里编写表现层的逻辑都是绝对失效的
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

		// =====================================================================
		// 【时序大一统】：这里就是新世界物理构建完毕的绝对瞬间！
		// 趁现在 UI 还在屏幕上死死遮挡，立刻由大管家亲自下令，完成跨界坐标折叠！
		if (UMyMapTravelSubsystem* TravelSub = World->GetSubsystem<UMyMapTravelSubsystem>())
		{
			TravelSub->SnapPlayerToDestination();
		}
		// =====================================================================

		// 宣布就绪！UI 此时接收到该状态变动信号，瞬间触发平滑提速变轨逻辑
		bEngineIsReady = true;

		if (bMinTimeElapsed)
		{
			// 如果大管家定下的最低加载时间已经到了，立刻强制关门触发 UI 退场
			CheckAndHideLoadingScreen();
		}
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

void UMyGameInstance::OnSameMapScreenOffFinished(TWeakObjectPtr<AActor> TeleportingActor, FTransform TargetTransform)
{
	// 【表现层铁律】：客机本地坚决不执行任何 TeleportTo 物理操作！
	// 此时本地客户端只负责拉好了纯黑的遮罩。
	// 立即向服务器发送 Client Ready 信号，耐心等待服务器在绝对黑幕下像搬运棋子一样调度我们的肉体。

	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (UMyMapTravelNetComponent* NetComp = PC->FindComponentByClass<UMyMapTravelNetComponent>())
		{
			NetComp->Server_NotifyClientReady();
		}
	}
}

void UMyGameInstance::FinalizeSameMapTransition()
{
	// 1. 【核心对接】：在物理坐标折叠、镜头视口刚切换的同一绝对物理帧，通知子系统进行数据层的真实切换！
	// 此时屏幕处于百分之百纯黑状态，玩家绝对看不到脚下地板被卸载和瞬间刷新的过程。
	// 安全获取世界上下文，准备进行全局子系统的调用
	if (UWorld* World = GetWorld())
	{
		// 跨系统通信：精准抓取负责大世界底层数据层流送的大一统传送子系统
		if (UMyMapTravelSubsystem* TravelSub = World->GetSubsystem<UMyMapTravelSubsystem>())
		{
			// 核心斩杀：下达换地指令！趁着 UI 纯黑，瞬间卸载远端地板并加载新区域，实现视觉 0 穿帮
			TravelSub->CommitSameMapDataLayer();
		}
	}

	// 2. 【核心排毒】：彻底物理抹杀那个死死挡住屏幕的纯黑遮罩！
	// 检查当前内存中是否还残留着强引用锁定的大黑幕 UI
	if (ActiveScreenOffUI)
	{
		// 毫不留情地从视口层级中物理根除黑幕，为接下来的加载进度条腾出空间
		ActiveScreenOffUI->RemoveFromParent();

		// 强制断开引用链，将黑幕 UI 的残骸移交引擎底层的垃圾回收系统 (GC)
		ActiveScreenOffUI = nullptr;
	}

	// 3. 拉起正常的伪加载 UI（进度条出现）
	// 传入 nullptr 空指针以触发大管家底层的字典查表机制，自动拉起目标区域专属的“睁眼/加载”UI
	PlayLoadingPhaseUI(nullptr);

	// 4. 【核心排毒】：同图无需硬盘加载，瞬间宣告底层就绪！进度条 UI 将直接提速满载！
	// 状态机欺骗：同图穿梭没有跨地图漫游那漫长的硬盘反序列化过程，
	// 直接强行点亮引擎 Ready 信号！UI 进度条接收到此信号后，内部电池会直接平滑提速冲刺到 100%。
	bEngineIsReady = true;

	// 5. 校验关门时间（防呆）
	// 双重锁核验：虽然引擎瞬间 Ready，但必须校验设计师定下的“最短观赏时间”是否也已耗尽
	if (bMinTimeElapsed)
	{
		// 双锁全开，满足所有关门条件！直接发送撤退信号，触发进度条 UI 的退场动画与最终物理解穴
		CheckAndHideLoadingScreen();
	}
}

#pragma endregion
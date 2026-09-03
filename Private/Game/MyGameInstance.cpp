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
	// 💥【采纳优化 1：安全解绑】：抛弃 AddLambda 捕获 this！改用 AddUObject 绑定专属回调。
	// 完美解决 PIE 结束或大管家销毁后，引擎触发此委托导致的 Access Violation 硬闪退！
	// ==============================================================================
	FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &UMyGameInstance::HandlePreLoadMap);

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

void UMyGameInstance::HandlePreLoadMap(const FString& MapName)
{
	// 💥【硬跳转/飞线兜底防线】：由引擎全局委托 PreLoadMap 触发。
	// 当引擎即将毁灭旧世界加载新地图时，如果是房主建房或客机飞线等非无缝漫游情况，
	if (UWorld* World = GetWorld())
	{
		// 引擎级别的容错：只要不是在正常的无缝漫游中，就代表是硬跳转。
		// 此时立刻复用无缝的清洗逻辑，赶在旧世界彻底粉碎前将脏数据彻底洗净！
		if (!World->IsInSeamlessTravel())
		{
			HandleStartTravel(World);
		}
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
		// 💥【采纳优化 3：斩断动画强引用】：UMG 播放动画时直接 Remove 会被 Latent Action 死死咬住。
		// 处决前必须强停所有动画，彻底消灭 "Object from PIE level still referenced" 闪退巨坑！
		ActiveLoadingScreenUI->StopAllAnimations();

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
		// 💥【架构解耦补充】：调用熄屏 UI 专属卸载接口，强停动画并从底层渲染树物理抹杀自身
		ActiveScreenOffUI->DismissScreenOffUI();

		ActiveScreenOffUI = nullptr;
	}

	// 归还控制权，执行父类的原生销毁
	Super::Shutdown();
}

void UMyGameInstance::OnStart()
{
	Super::OnStart();

	// 【核心修复】：解决 PIE (编辑器播放) 模式下，PostLoadMapWithWorld 从不触发导致的死锁
	if (UWorld* World = GetWorld())
	{
		if (World->IsPlayInEditor())
		{
			// 手动推一把，启动大管家的落地与 UI 收尾管线！
			HandleEndTravel(World);
		}
	}
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

void UMyGameInstance::ClearVisitedMaps()
{
	// 💥【内存管理防线】：应对超大规模项目的长线游玩带来的内存膨胀。
	// VisitedMaps 作为 TSet 会随玩家不断探索大世界而无限堆积哈希数据。
	// 这里直接调用 Empty() 而不是 Reset()，目的是强迫 UE 物理释放哈希表底层分配的内存块，
	// 建议在玩家返回主菜单、更换存档或彻底脱离大世界玩法时调用，将内存冗余彻底绞杀。
	VisitedMaps.Empty();
	UE_LOG(LogTemp, Warning, TEXT("🧹 [内存管理] VisitedMaps 已安全清空，底层内存块已释放，消灭内存冗余。"));
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

void UMyGameInstance::HandleScreenOffCovered()
{
	// 💥 【连通子系统】：UI 已经彻底黑透！
	// 【法则1：同图管线】如果目标名字为空，且绝对不是正在首次飞线加入大厅的客机 (!bIsClientInitialJoin)，则 100% 判定为同图漫游！
	// （注：本地冷启动开荒只有降临，没有退场动作，天然绝不会触发此函数，完美物理隔离。）
	// 同图漫游必须在此刻一边在本地瞬间拉起加载 UI 掩护，一边向服务器发射就绪 Ack！
	if (PendingTargetMapName.IsNone() && !bIsClientInitialJoin)
	{
		// 1. 同图专属黑幕拆除 (大管家自己负责清理自己的 UI，绝不让子系统代劳！)
		if (ActiveScreenOffUI)
		{
			// 💥【架构解耦补充】：调用熄屏 UI 专属卸载接口，强停动画并从底层渲染树物理抹杀自身
			ActiveScreenOffUI->DismissScreenOffUI();

			ActiveScreenOffUI = nullptr;
		}

		// 2. 瞬间拉起同图专属的加载进度条原画
		FName CurrentMapName = FName(*UGameplayStatics::GetCurrentLevelName(GetWorld(), true));
		FMapTransitionConfig Config = GetMapTransitionConfig(CurrentMapName);
		PlayLoadingPhaseUI(Config.LoadingScreenUIClass);

		// 💥 【核心修复 2：同图专属解穴】
		// 因为同图不切关卡，绝不会触发 HandleEndTravel，也绝不会触发引擎流送轮询！
		// 所以在这里拉起 UI 后，必须手动解开第一把锁，并尝试核验三锁合一！
		bEngineIsReady = true;
		CheckAndHideLoadingScreen();

		// 3. 呼叫传送子系统向服务器上报 Ack，进入 WaitingForShell
		if (UWorld* World = GetWorld())
		{
			if (UMyMapTravelSubsystem* TravelSub = World->GetSubsystem<UMyMapTravelSubsystem>())
			{
				TravelSub->NotifyLocalScreenOffFinished();
			}
		}
	}

	// 💥 没名字，且身上带着初次加入标识，必定是局外飞线连入的客机！
	else if (bIsClientInitialJoin)
	{
		// ==============================================================================
		// ⚠️【客机飞线注意】：绝不抢跑！维持大厅纯黑掩护，死等 ClientTravel 跨网！
		// ==============================================================================
		UE_LOG(LogTemp, Warning, TEXT("📺 [表现层] 客机飞线管线：大厅黑幕掩护就绪，死等底层 ClientTravel..."));
	}
	else
	{
		// ==============================================================================
		// ⚠️【跨图漫游注意】：
		// 如果是跨图，这里绝不能拉起UI（会被旧世界核平销毁），也绝不发信号！
		// 必须维持黑屏死等引擎执行 ServerTravel，将起跑线延后至 HandleEndTravel！
		// ==============================================================================
		UE_LOG(LogTemp, Warning, TEXT("📺 [表现层] 跨界漫游管线：旧世界黑幕掩护就绪，等待底层 ServerTravel..."));
	}
}

void UMyGameInstance::ResetTransitionLocks()
{
	bEngineIsReady = false;
	bMinTimeElapsed = false;
	bPhysicalReady = false; // 新增物理锁重置
	bIsHiding = false;
}

void UMyGameInstance::NotifyPhysicalReady()
{
	bPhysicalReady = true;
	CheckAndHideLoadingScreen();
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

			// 💥【大管家连线补充】：死死盯住熄屏 UI 的闭合事件，用以触发真实的物理握手！
			ScreenOffWidget->OnScreenOffCovered.AddDynamic(this, &UMyGameInstance::HandleScreenOffCovered);

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

	// 【视觉无缝补全】：加载 UI 上屏的瞬间，立刻解除底层的渲染断路器！
	if (BlackoutExt.IsValid())
	{
		BlackoutExt->bIsActive = false;
	}

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

			// 💥【大管家连线补充】：监听加载屏的退场完毕事件，绑定大管家的最终收尾核销与解穴函数！
			ActiveLoadingScreenUI->OnLoadingScreenHidden.AddDynamic(this, &UMyGameInstance::FinalizeLoadingScreenRemoval);

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
		// 💥【采纳优化 3 补充】：防范兜底强制销毁时的动画驻留！
		// 无论 UI 是正常播完还是被强行截断，处决前加上强停操作，100% 斩断 UMG 隐形引用链，绝不留内存死角。
		ActiveLoadingScreenUI->StopAllAnimations();

		// 从视口中物理抹除该 UI，彻底结束它在黑幕期间的屏幕霸权
		ActiveLoadingScreenUI->RemoveFromParent();

		// 强制置空指针，切断强引用链，将其残躯完全移交给虚幻底层的垃圾回收系统 (GC)
		ActiveLoadingScreenUI = nullptr;
	}

	// 状态机重置：清空挂起的目标地图名称缓存，确保下一次传送判定是一张纯净的白纸
	PendingTargetMapName = NAME_None;

	// 【新增】：一次大一统漫游彻底闭环，销毁跨界车票，防玩家死后重生依然触发幽灵传送！
	// (注：此代码目前被注释掉，若后续架构中存在车票生命周期泄漏风险，可随时在此解除封印)
	// 💥 【完美闭环修正】：在此处正式解除封印！物理折叠早已完成，UI已退场，在这里撕票才是绝对安全的！
	if (PendingTravelRoute)
	{
		PendingTravelRoute = nullptr;
		UE_LOG(LogTemp, Warning, TEXT("🎫 [跨图管线] UI退场完毕，大管家正式销毁跨界车票！"));
	}

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
		// 💥【采纳优化 3：斩断动画强引用】：UMG 播放动画时直接 Remove 会被 Latent Action 死死咬住。
		// 处决前必须强停所有动画，彻底消灭 "Object from PIE level still referenced" 闪退巨坑！
		ActiveLoadingScreenUI->StopAllAnimations();

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
		// 💥【架构解耦补充】：调用熄屏 UI 专属卸载接口，强停动画并从底层渲染树物理抹杀自身
		ActiveScreenOffUI->DismissScreenOffUI();

		ActiveScreenOffUI = nullptr;
	}

	// 统一调用重置：漫游开始，彻底重置所有状态锁与互斥锁，为新一轮加载做纯净准备
	ResetTransitionLocks();
}

void UMyGameInstance::HandleEndTravel(UWorld* NewWorld)
{
	if (!NewWorld) return;

	// ==============================================================================
	// 1. 【本地冷启动 / 副机被动落地判定】
	// GameInstance 是纯本地对象，绝对不参与网络同步！
	// 排除掉持有“客机初次加入标识”的情况后 (!bIsClientInitialJoin)，如果大管家手里没车票 (IsNone 为 true)，说明只有两种绝对情况：
	//   A. 主机侧：玩家刚刚双击 exe 启动游戏，或者局域网直连（真正的冷启动开荒）。
	//   B. 副机侧：局内副机是被引擎底层 ServerTravel 硬拽过来的，它没资格执行起飞前的查票代码，口袋必定是空的！
	// 因此，副机跨图落地时该判定必定为 true！借此豁免了后续的无缝过渡图拦截，顺理成章地混入公共 UI 唤醒管线！
	// ==============================================================================
	bool bIsLocalColdBoot = PendingTargetMapName.IsNone() && !bIsClientInitialJoin;

	// 提取当前真实落地的地图名（剔除 PIE 前缀）
	FString CleanCurrentMap = UGameplayStatics::GetCurrentLevelName(NewWorld, true);
	// 如果是本地冷启动，目标地图名就是当前地图；如果不是，就提取车票上的目标地图名
	FString CleanTargetMap = bIsLocalColdBoot ? CleanCurrentMap : FPackageName::GetShortName(PendingTargetMapName.ToString());

	// 2. 【核心防线】：精准跳过无缝漫游的过渡层 (TransitionMap)
	// 如果不是冷启动，且当前落地的地图根本不是我们要去的地图，说明我们掉进了过渡虚空！
	// 💥【新增客机豁免】：飞线客机初次落地时就算无跨图车票，也绝对不能通过下方的名字比对。
	// 此处必须用 !bIsClientInitialJoin 强行放行，防止其被当成过渡图误杀拦截，导致永远黑屏死锁！
	if (!bIsLocalColdBoot && !bIsClientInitialJoin && !CleanCurrentMap.Contains(CleanTargetMap))
	{
		UE_LOG(LogTemp, Warning, TEXT("⏭️ [表现层] 侦测到无缝过渡地图，跳过 UI 唤醒与黑幕解除: %s"), *CleanCurrentMap);
		return;
	}

	// 💥【核心修复：落地核销客机初次加入标识】
	// 落地真实地图后，如果身上有这个初次加入标识，就立刻核销！
	// 把它变成一个“局内人”，后续跟着主机跨图、同图漫游，再也不会触发大厅的误判逻辑！
	if (bIsClientInitialJoin)
	{
		bIsClientInitialJoin = false;
		UE_LOG(LogTemp, Warning, TEXT("🎫 [大管家] 客机初次加入令牌已核销，恢复常规大一统管线！"));
	}

	// 无论如何，只要落地真实的 Persistent Level，立刻解除底层的渲染断路器！
	if (BlackoutExt.IsValid())
	{
		BlackoutExt->bIsActive = false;
	}

	// 记录 Persistent Level 落地的确切物理时间戳
	PersistentLevelLoadTime = FPlatformTime::Seconds();

	// 3. 落地 UI 表现
	if (bIsLocalColdBoot)
	{
		// 删掉原本骗人的“开荒”日志，还原真相！
		UE_LOG(LogTemp, Warning, TEXT("🖥️ [大管家] 本地引擎冷启动/直连落地，初始化基础 UI: %s"), *CleanCurrentMap);
		// 狸猫换太子：塞入当前地图名，以便底层查字典拉起兜底的加载 UI
		PendingTargetMapName = FName(*CleanCurrentMap);
	}

	// Persistent Level 进内存后立刻拉起 UI 遮罩
	// 💥【法则2：跨图管线】到达新地图就一边本地拉起加载UI一边发信号！
	// 
	// 为什么这里没有同图 (法则1) 的逻辑？
	// 因为 HandleEndTravel 绑定的是 PostLoadMapWithWorld，只有加载新地图文件时才会触发！
	// 同图漫游永远进不来这里。所以此时身处此地的，必定是刚刚落地新世界的跨图玩家！
	PlayLoadingPhaseUI(nullptr);

	if (UWorld* World = GetWorld())
	{
		// 跨图落地进内存，发信号进入 WaitingForShell
		if (UMyMapTravelSubsystem* TravelSub = World->GetSubsystem<UMyMapTravelSubsystem>())
		{
			TravelSub->NotifyLocalScreenOffFinished();
		}

		// 自动化接管：开启高频雷达，每 0.1 秒轮询拷问引擎底层流送状态
		// 💥【采纳优化 2：雷达降频】：将轮询频率从 0.05f 放宽至 0.1f。
		// 大世界落地初期主线程满载，稍微放宽轮询能有效消除气泡卡顿。
		World->GetTimerManager().SetTimer(EngineReadyPollTimerHandle, this, &UMyGameInstance::PollEngineReadyStatus, 0.1f, true);
	}
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
		// 💥【时序致命修复】：必须先翻转自身的就绪状态，再呼叫外部总闸！
		// 物理总闸 CheckAndExecutePhysicalDeployment 内部会回调 GI->IsEngineReady() 索要凭证。
		// 如果不提前设为 true，总闸会因为验证失败而当场 return，导致死锁！
		// =====================================================================
		bEngineIsReady = true;

		// =====================================================================
		// 💥【修复与释疑】：引擎 Ready 后，通知子系统触发大一统物理总闸，绝不越权直调底层！
		// 
		// 为什么不能像以前一样，在本函数 (本地雷达端) 发现引擎加载完毕后，直接调用底层的 TravelSub->SnapPlayerToDestination()？
		// 
		// 因为在联机环境下，系统天然存在两条平行的触发管线：一条是客机发回 Ack 信号触发的【令牌网络端】(HandleDeploymentTokenUpdate)，
		// 另一条就是本函数所在的【本地雷达端】(PollEngineReadyStatus)。
		// 
		// 令牌网络端非常守规矩，它呼叫的是带有去重防线的【物理总闸】(CheckAndExecutePhysicalDeployment，内部第一行就有 bIsPhysicalLayoutReady 拦截)；
		// 而 SnapPlayerToDestination 则是极其底层的【无锁死逻辑】(一旦调用，必然执行物理搬运，并在结束后强行关闭开荒锁 bIsCurrentMapInitialBoot)。
		// 
		// 如果本地雷达端绕过总闸，直接调用这个死逻辑，就会在微秒级的时间差内发生致命的“双轨踩踏”：
		// 假设令牌网络端先完成了同步并触发总闸，完美执行了开荒原位夺舍，随后将开荒锁设为了 false；
		// 紧接着，本地雷达端也侦测到引擎加载完毕，如果它直调 SnapPlayerToDestination，就会完全无视总闸的拦截锁。
		// 此时，由于开荒锁已经被上一波关闭，雷达的这次越权直调会把原本完美的开荒图当成【动态排队图】，
		// 强行把玩家从原生假人的位置拽走，并套上 2.5D 的 120 偏移量排队阵型，彻底摧毁开荒现场！
		// 
		// 【大一统收束】：因此，必须把本地雷达也接入 CheckAndExecutePhysicalDeployment，
		// 让这两条管线在同一个总闸前汇合，利用 bIsPhysicalLayoutReady 锁完美挡下时序交织带来的重复越权指令！
		if (UMyMapTravelSubsystem* TravelSub = World->GetSubsystem<UMyMapTravelSubsystem>())
		{
			TravelSub->CheckAndExecutePhysicalDeployment();
		}
		// =====================================================================

		if (bMinTimeElapsed)
		{
			// 如果大管家定下的最低加载时间已经到了，立刻强制关门触发 UI 退场
			CheckAndHideLoadingScreen();
		}
	}
}

void UMyGameInstance::CheckAndHideLoadingScreen()
{
	// 【三锁合一核验】：引擎加载完毕 && 最短时间已过 && 物理排队完毕
	if (bEngineIsReady && bMinTimeElapsed && bPhysicalReady)
	{
		// 新增防线：互斥锁保护
		// 拦截极端情况下的高频冗余触发，退场指令有且只有一次生效的机会！
		if (!bIsHiding)
		{
			bIsHiding = true;

			UE_LOG(LogTemp, Warning, TEXT("🚀 [Triple-Lock] 三锁全通！物理/引擎/时间已对齐，执行 UI 退场。"));

			// 满足所有前置条件，正式执行 UI 隐藏
			HideFakeLoadingScreen();
		}
	}
}

#pragma endregion
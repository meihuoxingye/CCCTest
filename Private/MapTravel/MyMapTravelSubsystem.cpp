// Fill out your copyright notice in the Description page of Project Settings.

#include "MapTravel/MyMapTravelSubsystem.h"
#include "Engine/World.h"
// 【修改】：跨系统呼叫专职的流送子系统
#include "Streaming/MyDataLayerStreamingSubsystem.h" 
#include "WorldPartition/DataLayer/DataLayerAsset.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "Framework/Application/SlateApplication.h" 
#include "Engine/GameViewportClient.h"
#include "GameFramework/Character.h" // 【新增】：替换原来的 TopCharacter.h
#include "EnhancedInputComponent.h"
#include "GameFramework/PlayerState.h"
#include "Game/MyGameInstance.h"
#include "Misc/PackageName.h"
#include "Component/TimeDilationHubComponent.h"
#include "MapTravel/DataAsset/TeleportRoute.h"
#include "EngineUtils.h"
#include "MapTravel/Actors/MyUniversalDestination.h"
#include "GameFramework/CharacterMovementComponent.h"


// ==============================================================================
// 内部安全获取真实玩家控制器的工具函数 (防无缝传送假身)
// ==============================================================================
namespace
{
	APlayerController* GetRealPlayerController(UWorld* World)
	{
		// 防御性编程：世界指针为空直接返回
		if (!World) return nullptr;

		// 遍历当前世界中所有的玩家控制器
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			// 获取当前的 PlayerController 指针
			APlayerController* PC = It->Get();

			// 1. PC 必须有效
			// 2. 必须是 LocalController (防止联机模式下拿到远程客户端的代理控制器)
			// 3. 必须拥有 LocalPlayer (防止无缝漫游中，拿到还没被销毁的残留假身或幽灵)
			if (PC && PC->IsLocalController() && PC->GetLocalPlayer())
			{
				// 找到了唯一合法的真实本地控制器，返回
				return PC;
			}
		}

		// 没找到则返回空
		return nullptr;
	}
}


// ==============================================================================
// 生命周期与初始化 (Lifecycle & Initialization)
// ==============================================================================
#pragma region

void UMyMapTravelSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	// 调用父类初始化
	Super::Initialize(Collection);

	// 物理重置所有内部状态机参数与转场锁
	bIsTraveling = false;

	// 【修改】：重置同图数据层挂起池为空
	PendingSameMapDataLayer = nullptr;
}

void UMyMapTravelSubsystem::Deinitialize()
{
	// 【修改】：清空数据层挂起池
	PendingSameMapDataLayer = nullptr;

	// 安全获取当前世界
	if (UWorld* World = GetWorld())
	{
		// 防内存泄漏：清除本子系统挂在 TimerManager 上的所有定时器
		World->GetTimerManager().ClearAllTimersForObject(this);
	}

	// 调用父类反初始化完成收尾
	Super::Deinitialize();
}

void UMyMapTravelSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	// 调用父类逻辑
	Super::OnWorldBeginPlay(InWorld);

	// 获取当前刚落地的世界地图的原始长名称
	FString CurrentMapName = InWorld.GetMapName();

	// 获取大管家 GameInstance
	if (UMyGameInstance* GI = InWorld.GetGameInstance<UMyGameInstance>())
	{
		// 提取纯净的地图短名用于对比
		FString CleanTargetMap = FPackageName::GetShortName(GI->PendingTargetMapName.ToString());
		FString CleanCurrentMap = FPackageName::GetShortName(CurrentMapName);

		// 过滤过渡地图：如果没有挂起的目标，或者当前落地地图不是期望地图
		if (GI->PendingTargetMapName.IsNone() || !CleanCurrentMap.Contains(CleanTargetMap))
		{
			// 直接返回，不执行后续新世界的任何初始化逻辑
			return;
		}
	}
}

#pragma endregion


// ==============================================================================
// 大一统传送路由中心 (Universal Routing Hub)
// ==============================================================================
#pragma region

void UMyMapTravelSubsystem::RegisterSameMapDestination(UTeleportRoute* Route, AActor* DestinationActor, const FTransform& TargetTransform, UDataLayerAsset* BoundDataLayer)
{
	// 基础安全防线：拒绝空头支票，传入的路由资产与接机实体必须双双有效
	if (!Route || !DestinationActor) return;

	// 架构级防呆：捕获并镇压由于关卡策划配置失误导致的多对一冲突
	if (SameMapDestinationRegistry.Contains(Route))
	{
		// 提取冲突字典中已存在的旧注册信息，准备用于拼装警告日志
		FDestinationRegistrationInfo ConflictedInfo = SameMapDestinationRegistry[Route];

		// 安全萃取旧目标的名称：如果旧实体还活着就取其真名，如果已经被销毁则显示默认提示，防止野指针解引用崩溃
		FString OldActorName = ConflictedInfo.RegistrySource.IsValid() ? ConflictedInfo.RegistrySource->GetName() : TEXT("已失效实体");

		// 获取当前试图强行注册的“非法”入侵者的名称
		FString NewActorName = DestinationActor->GetName();

		// 向开发者输出极度醒目的红色警告阵列，强制暴露脏数据
		UE_LOG(LogTemp, Error, TEXT("=========================================================================="));
		UE_LOG(LogTemp, Error, TEXT("[大一统传送系统] 致命冲突！路由资产 [%s] 被多个目标点同时监听！"), *Route->GetName());
		UE_LOG(LogTemp, Error, TEXT(" -> 已注册生效的守卫点: [%s]"), *OldActorName);
		UE_LOG(LogTemp, Error, TEXT(" -> 试图二次篡改的侵入点: [%s] (此入侵已被系统物理隔离并抛弃！)"), *NewActorName);
		UE_LOG(LogTemp, Error, TEXT("=========================================================================="));

		// 发生冲突时直接打断执行，拒绝脏数据入库
		return;
	}

	// 实例化一个新的注册信息结构体，准备打包数据
	FDestinationRegistrationInfo NewInfo;

	// 绑定接机点的物理实体（弱指针），用于后续安全寻址与生命周期追踪
	NewInfo.RegistrySource = DestinationActor;

	// 封存该接机点在世界中的绝对物理坐标与旋转信息
	NewInfo.TargetTransform = TargetTransform;

	// 封存该接机点绑定的数据层，供后续瞬移时的滑动窗口流送使用
	NewInfo.BoundDataLayer = BoundDataLayer; // 【新增保存目标数据层】

	// 将打包好的目标点数据正式编入高速缓存字典，以路由资产为 Key 建立映射
	SameMapDestinationRegistry.Add(Route, NewInfo);
}

void UMyMapTravelSubsystem::UnregisterSameMapDestination(UTeleportRoute* Route)
{
	// 物理清洗机制：当地图中的接机点被摧毁或卸载时，将其从高速字典中精准剥离，防野指针崩溃
	if (Route)
	{
		// 从字典中抹除该路由的映射关系，释放内存并解除监听
		SameMapDestinationRegistry.Remove(Route);
	}
}

void UMyMapTravelSubsystem::ExecuteUniversalTravel(AActor* TeleportingActor, UTeleportRoute* TargetRoute)
{
	// 基础防线：拦截无效的传送物理实体或未配置的空路由资产
	if (!TeleportingActor || !TargetRoute) return;

	// 打印大一统传送系统的入口分割线与寻址起点日志
	UE_LOG(LogTemp, Warning, TEXT("[MapTravelLog] ==============================================="));
	UE_LOG(LogTemp, Warning, TEXT("[MapTravelLog] 🚀 玩家踩上传送门！开始大一统路由寻址。收到的目标路线: [%s]"), *TargetRoute->GetName());

	// 第一路由优先级：如果目标路由的接机点在当前内存字典中，直接劫持为同地图极速穿梭
	if (SameMapDestinationRegistry.Contains(TargetRoute))
	{
		// 命中本地字典，输出同地图传送判定日志
		UE_LOG(LogTemp, Warning, TEXT("[MapTravelLog] -> 🔍 字典查表成功：本地有监听点，判定为【同地图传送】！"));

		// 派发给同图专属传送管线处理
		ExecuteSameMapTravel(TeleportingActor, TargetRoute);

		// 寻址完成，直接阻断后续逻辑
		return;
	}

	// 未命中本地字典，输出跨地图传送判定日志
	UE_LOG(LogTemp, Warning, TEXT("[MapTravelLog] -> 🔍 字典查表失败：本地无此目标点，判定为【跨地图漫游】！"));

	// 第二路由优先级：本地查无此人，断定为跨界航行，准备查阅资产内部的目标地图
	UWorld* World = GetWorld();

	// 安全拦截：如果世界上下文失效，直接中止寻址
	if (!World) return;

	// 提取全局大管家，准备移交跨地图流送指令
	if (UMyGameInstance* GI = World->GetGameInstance<UMyGameInstance>())
	{
		// 校验路由资产内部是否确实配置了跨图的目的地（软引用是否为空）
		if (!TargetRoute->TargetMap.IsNull())
		{
			// 铸造跨界车票并交由全局大管家保管，留待落地后验证接机点
			GI->PendingTravelRoute = TargetRoute;

			// 从软引用萃取真实地图包名，移交跨地图无缝流送管线
			FString TargetMapName = TargetRoute->TargetMap.GetAssetName();

			// 输出跨界车票发放成功的日志
			UE_LOG(LogTemp, Warning, TEXT("[MapTravelLog] -> 🎫 成功购买跨图车票: [%s]，准备前往新世界: [%s]"), *TargetRoute->GetName(), *TargetMapName);

			// 正式启动底层跨地图传送流送管线
			ExecuteMapTravel(*TargetMapName);
		}
		else
		{
			// 策划配置失误：既不在本地字典，也没填跨图关卡名，抛出致命错误日志
			UE_LOG(LogTemp, Error, TEXT("[大一统传送系统] 寻路瘫痪！路由资产 [%s] 既不在本图监听字典中，也未配置目标跨界地图！"), *TargetRoute->GetName());
			UE_LOG(LogTemp, Error, TEXT("[MapTravelLog] ❌ 致命错误：路由资产 [%s] 中没有配置任何目标地图！"), *TargetRoute->GetName());
		}
	}
}

void UMyMapTravelSubsystem::ExecuteSameMapTravel(AActor* TeleportingActor, UTeleportRoute* TargetRoute)
{
	// 时序锁与合法性首重校验：防玩家连踩触发器导致状态机重入，并确保传入参数绝不为空
	if (bIsTraveling || !TeleportingActor || !TargetRoute) return;

	// 字典二重查验：确保目标接机点在此刻确切存活于本地内存字典中
	if (!SameMapDestinationRegistry.Contains(TargetRoute)) return;

	// 锁定全局转场状态，宣布进入不可逆的传送管线
	bIsTraveling = true;

	// 安全提取世界上下文
	UWorld* World = GetWorld();
	if (!World)
	{
		// 极端情况下的逃生舱：如果世界失效，必须解开转场锁防止死锁
		bIsTraveling = false;
		return;
	}

	// 从字典中提取出接机点在世界中的绝对物理坐标与旋转信息
	FTransform TargetTransform = SameMapDestinationRegistry[TargetRoute].TargetTransform;

	// 提取该接机点所绑定的目标大世界数据层
	UDataLayerAsset* TargetDataLayer = SameMapDestinationRegistry[TargetRoute].BoundDataLayer;

	// 【核心新增】：在触发同地图传送黑幕及逻辑前，立刻刷新滑动窗口，趁着黑屏转场的间隙把数据层流送加载出来！
	if (TargetDataLayer)
	{
		// 输出预热日志
		UE_LOG(LogTemp, Warning, TEXT("[MapTravelLog] 🔄 同图传送：触发数据层滑动窗口，开始加载目标区域并卸载远端"));

		// 【修改】：呼叫流送子系统执行底层预热加载
		if (UMyDataLayerStreamingSubsystem* StreamingSub = World->GetSubsystem<UMyDataLayerStreamingSubsystem>())
		{
			// 调用预热函数：把目标艺术层丢进后台静默加载，但不唤醒碰撞和逻辑
			StreamingSub->PreheatZoneBackground(TargetDataLayer);
		}

		// 将目标数据层塞入挂起池，等待黑幕完全闭合时再由大管家触发交接
		PendingSameMapDataLayer = TargetDataLayer;
	}

	// 物理层静默：强制抹平全局时空流速，为接下来的点穴做准备
	UGameplayStatics::SetGlobalTimeDilation(World, 1.0f);

	// 提取真实的本地玩家控制器
	if (APlayerController* PC = GetRealPlayerController(World))
	{
		// 强制抹平玩家控制器的个体时间流速，防止时空残留导致逻辑异常
		PC->CustomTimeDilation = 1.0f;

		// 逻辑层彻底点穴：剥夺玩家控制器的所有输入响应能力
		PC->DisableInput(PC);

		// 创建纯 UI 输入模式上下文，切断游戏视口的交互
		FInputModeUIOnly InputMode;

		// 解除鼠标对视口的锁定，防止黑屏期间鼠标被死锁在屏幕中心或边界
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);

		// 将组装好的 UI 输入模式硬塞给玩家控制器
		PC->SetInputMode(InputMode);

		// 【完美契合】：获取角色肉体基类，进行极致优雅的物理冻结
		if (APawn* Pawn = PC->GetPawn())
		{
			// 同步抹平肉体的时间流速
			Pawn->CustomTimeDilation = 1.0f;

			// 再次双重保险：剥夺肉体基类的输入接收
			Pawn->DisableInput(PC);

			// 【核心新增】：极致点穴！彻底清空速度并掐断重力计算，角色被物理死锁在虚空中，杜绝掉落
			if (ACharacter* PlayerCharacter = Cast<ACharacter>(Pawn))
			{
				// 提取 CMC 组件
				if (UCharacterMovementComponent* CMC = PlayerCharacter->GetCharacterMovement())
				{
					// 关闭物理驱动：挂起所有重力和摩擦力计算
					CMC->DisableMovement();
				}
			}
		}
	}

	// 【极致解耦】：时序移交！抛弃本类一切脏 Timer，将物理坐标抛给拥有严密状态机的大管家 (GameInstance)
	if (UMyGameInstance* GI = World->GetGameInstance<UMyGameInstance>())
	{
		// 由大管家亲自执行遮罩淡入、绝对坐标折叠和解穴逻辑
		GI->ExecuteSameMapTransition(TeleportingActor, TargetTransform);
	}
}

void UMyMapTravelSubsystem::CommitSameMapDataLayer()
{
	// 当大管家的 Timer 到期，并在 OnSameMapScreenOffFinished（屏幕 100% 纯黑）中回调本接口时，执行数据层斩杀
	if (PendingSameMapDataLayer)
	{
		// 输出斩杀与替换确认日志
		UE_LOG(LogTemp, Warning, TEXT("[MapTravelLog] 🔲 黑幕已就位！执行同图数据层真实切换与远端卸载"));

		// 【修改】：呼叫流送子系统刷新滑动窗口
		if (UMyDataLayerStreamingSubsystem* StreamingSub = GetWorld()->GetSubsystem<UMyDataLayerStreamingSubsystem>())
		{
			// 瞬间刷新窗口算法：利用当前身处的目标地块重新计算远近维度，激活新区域，强行物理级卸载老区域
			// 同图传送是在黑幕下进行的，传入 true 强制刷新纹理 MIP
			StreamingSub->RefreshSlidingWindow(PendingSameMapDataLayer, true);
		}

		// 清空同图数据层挂起池，完成本次数据流转的历史使命
		PendingSameMapDataLayer = nullptr;
	}
}

#pragma endregion


// ==============================================================================
// 核心跳转管线 (Core Travel Pipeline)
// ==============================================================================
#pragma region

void UMyMapTravelSubsystem::ExecuteMapTravel(FName TargetLevelName)
{
	// 转场状态互斥锁：防止玩家在黑屏淡出期间狂按按键或连踩触发器导致状态机重复执行
	if (bIsTraveling) return;

	// 锁定全局转场状态，宣布进入不可逆的传送管线
	bIsTraveling = true;

	// 安全获取当前世界上下文
	UWorld* World = GetWorld();

	// 终极防呆校验：如果世界不存在，或者传入的目标关卡名为空，直接中止并解锁状态
	if (!IsValid(World) || TargetLevelName.IsNone())
	{
		bIsTraveling = false;
		return;
	}

	// 全局物理破片盾牌：第一时间恢复法则，防止转场 Timer 因全局变慢而被无限拉长
	UGameplayStatics::SetGlobalTimeDilation(World, 1.0f);

	// 安全查验引擎和游戏视口是否存在
	if (GEngine && GEngine->GameViewport)
	{
		// 瞬间剥夺物理输入控制权
		GEngine->GameViewport->SetIgnoreInput(true);

		// 清空键盘焦点，阻断残余UI按键（防止玩家恰好按在某个隐形或未销毁的按钮上）
		FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::Cleared);

		// 释放所有指针捕获，防止鼠标锁定导致UI失效
		FSlateApplication::Get().ReleaseAllPointerCapture();
	}

	// 遍历并压制当前世界中所有的玩家控制器
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		// 提取真实的玩家控制器指针
		if (APlayerController* PC = It->Get())
		{
			// 时间领主防线：只对灵魂进行操作，安全关闭时间组件内部状态机
			if (UTimeDilationHubComponent* TimeComp = PC->GetComponentByClass<UTimeDilationHubComponent>())
			{
				// 强行重置时间组件，防止带着子弹时间进入新世界
				TimeComp->ForceResetTime();
			}

			// 消除灵魂个体的流速残留，防止影响新世界
			PC->CustomTimeDilation = 1.0f;

			// 清理该控制器上绑定的全部定时器，切断过场期间的异步干扰
			World->GetTimerManager().ClearAllTimersForObject(PC);

			// 声明组件数组，准备抓取控制器下属的所有组件
			TArray<UActorComponent*> UIComponents;
			// 获取控制器身上的所有组件
			PC->GetComponents(UIComponents);

			// 遍历控制器的每一个组件
			for (UActorComponent* Comp : UIComponents)
			{
				// 清理控制器下属组件的定时器，防止组件在后台作妖
				World->GetTimerManager().ClearAllTimersForObject(Comp);
			}

			// 尝试获取本地玩家的增强输入子系统
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
			{
				// 清空增强输入系统的映射上下文，防止黑屏期间发生轴输入叠加
				Subsystem->ClearAllMappings();
			}

			// 物理层清除粘连按键状态，防落地后自动往前跑
			PC->FlushPressedKeys();

			// 逻辑层点穴：禁止控制器接受任何输入，绝不调用 UnPossess 以维持附身关系
			PC->DisableInput(PC);
			// 忽略移动轴输入
			PC->SetIgnoreMoveInput(true);
			// 忽略视角轴输入
			PC->SetIgnoreLookInput(true);

			// 尝试获取控制器当前附身的肉体 Pawn
			if (APawn* PlayerPawn = PC->GetPawn())
			{
				// 肉体只配被重置个体时间，绝不去肉体蓝图里找全局时间组件
				PlayerPawn->CustomTimeDilation = 1.0f;

				// 物理层点穴：关闭碰撞防止穿模或掉落，摄像机完美维持原机位
				PlayerPawn->SetActorEnableCollision(false);

				// 【真正遗漏的核心补丁】：跨地图踩门的一瞬间，立刻物理死锁旧肉体！防止黑幕起步时滑行或掉落
				if (ACharacter* PlayerCharacter = Cast<ACharacter>(PlayerPawn))
				{
					// 提取角色移动组件 (CMC)
					if (UCharacterMovementComponent* CMC = PlayerCharacter->GetCharacterMovement())
					{
						// 掐断重力与动力，角色被完全冻结
						CMC->DisableMovement();
					}
				}
			}

			// 重迎免死金牌：如果控制器不属于持久关卡
			if (PC->GetLevel() != World->PersistentLevel)
			{
				// 强行将控制器挂载到持久关卡，防 World Partition 导致 0x30 闪退
				PC->Rename(nullptr, World->PersistentLevel);
			}

			// 重迎免死金牌：如果玩家状态 (PlayerState) 有效且不属于持久关卡
			if (PC->PlayerState && PC->PlayerState->GetLevel() != World->PersistentLevel)
			{
				// 强行将玩家状态挂载到持久关卡，防无缝漫游期间数据链断裂
				PC->PlayerState->Rename(nullptr, World->PersistentLevel);
			}
		}
	}

	// 尝试获取全局大管家 GameInstance
	UMyGameInstance* GI = World->GetGameInstance<UMyGameInstance>();

	// 如果大管家不存在，放弃跨地图跳转并解锁状态
	if (!GI)
	{
		bIsTraveling = false;
		return;
	}

	// 提取当前旧世界的关卡名称，用于查表获取其专属的退场时间
	FName CurrentMapName = FName(*UGameplayStatics::GetCurrentLevelName(World, true));

	// 查字典：获取当前地图对应的转场配置信息
	FMapTransitionConfig OutConfig = GI->GetMapTransitionConfig(CurrentMapName);

	// 核心数据驱动：直接记下目标地图名，落地后大管家自己会根据名字去查字典
	GI->PendingTargetMapName = TargetLevelName;

	// 通过大管家配置拉起熄屏 UI，传入软引用与设计师配置的淡出时间
	GI->PlayScreenOffPhaseUI(OutConfig.ScreenOffUIClass, OutConfig.ScreenOffDuration);

	// 计算系统安全延迟：取系统强锁底线（0.1s）与 UI 动画时长的最大值
	float SafeTravelDelay = FMath::Max(GI->SystemSafeDelay, OutConfig.ScreenOffDuration);

	// 声明一个定时器句柄，用于挂起最终的跨地图跳转指令
	FTimerHandle TravelTimerHandle;

	// 【核心修复】：使用 TWeakObjectPtr 包装世界指针，防止 2 秒等待期内世界被意外销毁导致野指针崩溃
	TWeakObjectPtr<UWorld> WeakWorld(World);

	// 开启定时器，等待 UI 黑幕完全闭合后执行匿名回调
	World->GetTimerManager().SetTimer(TravelTimerHandle, [WeakWorld, TargetLevelName]()
		{
			// 唤醒时重新安全提取强引用，如果内存已经被销毁，这里会安全返回 nullptr
			if (UWorld* SafeWorld = WeakWorld.Get())
			{
				// 真正起航：命令引擎底层开启无缝旅行管线，跨越位面
				SafeWorld->ServerTravel(TargetLevelName.ToString());
			}
		}, SafeTravelDelay, false);
}

void UMyMapTravelSubsystem::RestorePlayerInput()
{
	// 安全获取世界上下文
	UWorld* World = GetWorld();
	if (!World) return;

	// 彻底解除视口级别的输入忽略屏蔽
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->SetIgnoreInput(false);
	}

	// 核心解穴：安全获取本地真实的玩家控制器
	APlayerController* PC = GetRealPlayerController(World);
	if (PC)
	{
		// 恢复控制器本身的输入响应
		PC->EnableInput(PC);

		// 清理黑幕期间玩家盲按留下的残留物理按键状态
		PC->FlushPressedKeys();

		// 恢复为正常的游戏与UI混合输入模式
		FInputModeGameAndUI InputMode;

		// 解除鼠标对视口的强制锁定
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);

		// 确保捕获期间不隐藏鼠标光标
		InputMode.SetHideCursorDuringCapture(false);

		// 应用输入模式
		PC->SetInputMode(InputMode);

		// 恢复肉体的响应
		if (APawn* Pawn = PC->GetPawn())
		{
			// 恢复肉体的按键接收
			Pawn->EnableInput(PC);

			// 恢复肉体的物理碰撞
			Pawn->SetActorEnableCollision(true);

			// 【核心新增】：解穴！黑幕已散，目标地板已就绪，恢复物理引擎的重力与行走计算
			if (ACharacter* PlayerCharacter = Cast<ACharacter>(Pawn))
			{
				if (UCharacterMovementComponent* CMC = PlayerCharacter->GetCharacterMovement())
				{
					CMC->SetMovementMode(MOVE_Walking);
				}
			}
		}

		// 强制将底层的用户焦点重新对齐到游戏视口
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
	}

	// 彻底完成管线，解除转场锁
	bIsTraveling = false;
}

void UMyMapTravelSubsystem::SnapPlayerToDestination()
{
	// 安全获取当前世界上下文
	UWorld* World = GetWorld();
	if (!World) return;

	// 尝试获取全局大管家 (GameInstance)
	if (UMyGameInstance* GI = World->GetGameInstance<UMyGameInstance>())
	{
		// 验证通行证：仅当大管家手中确实持有跨界车票 (PendingTravelRoute) 时才触发后续管线
		if (GI->PendingTravelRoute)
		{
			// 声明目标接机点指针，准备进行全图搜索
			AMyUniversalDestination* TargetDest = nullptr;

			// 此时新世界的 Persistent Level 已 Ready，目标必然在内存中，执行全局 Actor 扫盘
			for (TActorIterator<AMyUniversalDestination> It(World); It; ++It)
			{
				// 提取当前遍历到的接机点
				if (AMyUniversalDestination* Dest = *It)
				{
					// 校验匹配：如果该接机点的监听列表中，包含大管家手里的这张车票
					if (Dest->ListeningRoutes.Contains(GI->PendingTravelRoute))
					{
						// 锁定目标接机点
						TargetDest = Dest;
						// 寻址成功，立刻跳出高耗时的遍历循环
						break;
					}
				}
			}

			// 如果成功找到了对应的接机点
			if (TargetDest)
			{
				// 【核心新增】：跨图落地时，立即根据目标点绑定的数据层刷新内存状态，触发卸载与加载！
				if (TargetDest->BoundDataLayer)
				{
					// 记录数据层滑动窗口触发日志
					UE_LOG(LogTemp, Warning, TEXT("[MapTravelLog] 🔄 跨图落地：立刻刷新目标点的数据层滑动窗口"));

					// 【修改】：呼叫流送子系统，强制拉起目标地块的硬盘流送任务，确保黑幕散去前地板加载完毕
					if (UMyDataLayerStreamingSubsystem* StreamingSub = World->GetSubsystem<UMyDataLayerStreamingSubsystem>())
					{
						// 跨图落地是在黑幕下进行的，传入 true 强制刷新纹理 MIP
						StreamingSub->RefreshSlidingWindow(TargetDest->BoundDataLayer, true);
					}
				}

				// 核心安全获取：提取绝对真实的本地玩家控制器，防死无缝漫游产生的幽灵假身
				if (APlayerController* PC = GetRealPlayerController(World))
				{
					// 提取玩家控制器当前附身的新世界肉体 (Pawn)
					if (APawn* Pawn = PC->GetPawn())
					{
						// 提取目标点的绝对坐标，Z轴强制增加 15cm 的高度冗余，作为完美防穿模的下落空间
						FVector SafeLocation = TargetDest->GetActorLocation() + FVector(0.0f, 0.0f, 15.0f);

						// 提取目标点设定的视口朝向
						FRotator SafeRotation = TargetDest->GetActorRotation();

						// 趁着现在屏幕全黑，强行折叠宇宙坐标，将玩家肉体瞬移至接机点！
						// false, true 分别代表：不进行碰撞测试(强扫)，且瞬移后维持物理速度(稍后会手动清空)
						Pawn->TeleportTo(SafeLocation, SafeRotation, false, true);

						// 极其关键：强行把玩家控制器的相机视角也拧过去，防止镜头滞后导致的平移拖影！
						PC->SetControlRotation(SafeRotation);

						// 【核心防坠落点穴】：跨图落地悬浮锁死！
						// 刚刚生成的新肉体被传送到 5 公里外，此时脚下的数据层地板还没流送过来，
						// 必须在这里立刻点穴，让新肉体悬浮在纯黑的虚空中绝对静止等待！
						if (ACharacter* PlayerCharacter = Cast<ACharacter>(Pawn))
						{
							// 提取角色移动组件 (CMC)
							if (UCharacterMovementComponent* CMC = PlayerCharacter->GetCharacterMovement())
							{
								// 物理闭包：挂起所有重力与动力计算，彻底冻结肉体
								CMC->DisableMovement();
							}
						}

						// 记录成功斩杀旧坐标并落地新坐标的胜利日志
						UE_LOG(LogTemp, Warning, TEXT("[MapTravelLog] 💥 黑幕掩护瞬移成功！强行穿插至目标路由: %s"), *GI->PendingTravelRoute->GetName());
					}
				}
			}

			// 物理坐标锁死完成，彻底撕毁车票！
			// 防止玩家在新图死后重生，大管家手里的车票还在，导致无限幽灵传送的恶性 Bug
			GI->PendingTravelRoute = nullptr;
		}
	}
}

#pragma endregion
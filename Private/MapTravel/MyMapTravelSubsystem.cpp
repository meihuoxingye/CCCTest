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

#include "GameFramework/GameModeBase.h"
#include "Game/MyGameModeBase.h"
#include "Character/TopCharacter.h"

#include "Components/CapsuleComponent.h"

#include "MapTravel/NetComponent/MyMapTravelNetComponent.h"

#include "GameFramework/PlayerStart.h" // 【新增】：解决 APlayerStart 编译器不认识的报错

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

void UMyMapTravelSubsystem::RegisterCharacterToCache(ATopCharacter* Character)
{
	if (Character)
	{
		GlobalCharacterCache.Add(Character);
	}
}

void UMyMapTravelSubsystem::UnregisterCharacterFromCache(ATopCharacter* Character)
{
	if (Character)
	{
		GlobalCharacterCache.Remove(Character);
	}
}

void UMyMapTravelSubsystem::RegisterSameMapDestination(UTeleportRoute* Route, AActor* DestinationActor, const FTransform& TargetTransform, UDataLayerAsset* BoundDataLayer)
{
	// 基础安全防线：拒绝空头支票，传入的路由资产与接机实体必须双双有效
	if (!Route || !DestinationActor) return;

	// 利用 UE 迭代器安全清理因 DataLayer 卸载而变成野指针的接机点，防止内存泄漏与寻址错乱
	for (auto It = SameMapDestinationRegistry.CreateIterator(); It; ++It)
	{
		if (!It.Value().RegistrySource.IsValid())
		{
			It.RemoveCurrent();
		}
	}

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

	FVector GroundCorrectedLocation = TargetTransform.GetLocation();
	FHitResult GroundHit;
	// 向上偏移探测，防止接机点本身就嵌在地板里导致探测失败
	FVector TraceStart = GroundCorrectedLocation + FVector(0.f, 0.f, 50.f);
	FVector TraceEnd = GroundCorrectedLocation - FVector(0.f, 0.f, 100.f);

	FCollisionQueryParams TraceParams;
	TraceParams.AddIgnoredActor(TeleportingActor); // 忽略玩家自身

	// 向下打射线，寻找真实的 3D 物理表面
	if (World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, TraceParams))
	{
		// 将高度对齐到真实的 3D 物理表面，稍微抬高 2cm 防止碰撞体重叠
		GroundCorrectedLocation.Z = GroundHit.ImpactPoint.Z + 2.0f;
		TargetTransform.SetLocation(GroundCorrectedLocation);
	}

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

	// ==============================================================================
	// 【防御性架构实装补全：全军同步点穴】
	// 在触发黑幕和网络等待前，立刻在物理世界封印全队（包括所有 AI 队友）的运动状态！
	// 绝对防止在等待数据层预热和客机 UI 的这几秒钟里，队友因重力坠落或遭受物理推挤产生位移。
	// ==============================================================================
	if (AMyGameModeBase* GM = Cast<AMyGameModeBase>(World->GetAuthGameMode()))
	{
		for (ATopCharacter* Teammate : GM->FriendlyRoster)
		{
			if (Teammate && !Teammate->IsActorBeingDestroyed())
			{
				if (UCharacterMovementComponent* CMC = Teammate->GetCharacterMovement())
				{
					CMC->DisableMovement();
				}
			}
		}
	}

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

	// ==============================================================================
	// 【网络握手第一阶段：触发所有客户端拉起黑幕】
	// ==============================================================================
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* IterPC = It->Get())
		{
			if (UMyMapTravelNetComponent* NetComp = IterPC->FindComponentByClass<UMyMapTravelNetComponent>())
			{
				NetComp->Server_InitiateTransition(TargetTransform);
			}
		}
	}

	// ==============================================================================
	// 【网络握手第二阶段：高频轮询与超时防死锁】
	// ==============================================================================
	TWeakObjectPtr<UMyMapTravelSubsystem> WeakThis(this);
	TWeakObjectPtr<AActor> WeakTeleporter(TeleportingActor);

	// 记录开始轮询的绝对物理时间戳
	double StartTime = World->GetTimeSeconds();
	// 设定最长等待容忍度：5秒。超过此时间，视为客机网络断连，强制执行物理折叠防死锁！
	const double MaxTravelWait = 5.0;

	World->GetTimerManager().SetTimer(SyncWaitTimerHandle, [WeakThis, WeakTeleporter, TargetTransform, StartTime, MaxTravelWait]()
		{
			UMyMapTravelSubsystem* StrongThis = WeakThis.Get();
			UWorld* SafeWorld = StrongThis ? StrongThis->GetWorld() : nullptr;
			if (!StrongThis || !SafeWorld) return;

			bool bAllReady = true;

			// 检查当前局内所有有效玩家的同步组件状态
			for (FConstPlayerControllerIterator It = SafeWorld->GetPlayerControllerIterator(); It; ++It)
			{
				if (APlayerController* IterPC = It->Get())
				{
					if (UMyMapTravelNetComponent* NetComp = IterPC->FindComponentByClass<UMyMapTravelNetComponent>())
					{
						if (NetComp->GetSyncState() != ETravelSyncState::Ready)
						{
							bAllReady = false;
							break;
						}
					}
				}
			}

			// 🛡️ 超时兜底逻辑：计算已经死等了多久
			double ElapsedTime = SafeWorld->GetTimeSeconds() - StartTime;
			if (ElapsedTime >= MaxTravelWait)
			{
				UE_LOG(LogTemp, Error, TEXT("⚠️ [MapTravel] 同步握手超时 (已等待 %.1f 秒)！强制执行物理折叠以防止全队死锁。"), ElapsedTime);
				bAllReady = true; // 强制放行
			}

			// ==============================================================================
			// 【网络握手第三阶段：全员黑屏就绪，执行物理折叠！】
			// ==============================================================================
			if (bAllReady)
			{
				SafeWorld->GetTimerManager().ClearTimer(StrongThis->SyncWaitTimerHandle);

				// ==============================================================================
				// 【核心补全：同地图小队跟随瞬移！ (Same-Map Squad Teleport)】
				// 遵循“灵魂与躯壳分离”原则：客户端只负责黑屏，所有肉体的物理移动全由服务器统筹。
				// ==============================================================================
				if (AMyGameModeBase* GM = Cast<AMyGameModeBase>(SafeWorld->GetAuthGameMode()))
				{
					// 【核心修复】：提取目标地的绝对地板 Z 坐标，杜绝 90cm 悬空
					float BaseFloorZ = TargetTransform.GetLocation().Z;

					// 结合 O(1) 优化，提取黑名单
					TArray<AActor*> IgnoredActorsForTrace;
					for (const auto& WeakChar : StrongThis->GlobalCharacterCache)
					{
						if (ATopCharacter* Char = WeakChar.Get()) IgnoredActorsForTrace.Add(Char);
					}

					// 2.5D 专属排兵布阵！使用固定的左右交替站位，杜绝肉体重叠
					// 不再区分谁是主机、谁是副机、谁是AI。所有人都只是 FriendlyRoster 里的躯壳。
					float CurrentXOffset = 0.0f;
					bool bFlipOffset = true;

					// 直接遍历已经存在于当前地图上的队友肉体
					for (ATopCharacter* Teammate : GM->FriendlyRoster)
					{
						// 1. 基础安全校验
						if (!Teammate || Teammate->IsActorBeingDestroyed()) continue;

						// 左右交替偏移：基于目标点 TargetTransform
						FVector OffsetLoc = TargetTransform.GetLocation();
						OffsetLoc.X += (bFlipOffset ? CurrentXOffset : -CurrentXOffset);

						// 每排完一左一右，间距拉大
						if (!bFlipOffset || CurrentXOffset == 0.0f)
						{
							CurrentXOffset += 120.0f;
						}
						if (CurrentXOffset > 0.0f)
						{
							bFlipOffset = !bFlipOffset;
						}

						// ==============================================================================
						// 【同图探针升级：贴地短射线 + ECC_WorldStatic】
						// ==============================================================================
						FHitResult GroundHit;
						// 从预估地板的上方 50cm 往下探 100cm，避免从高空打中隐形门框
						FVector TraceStart = OffsetLoc;
						TraceStart.Z = BaseFloorZ + 50.0f;
						FVector TraceEnd = OffsetLoc;
						TraceEnd.Z = BaseFloorZ - 100.0f;

						FCollisionQueryParams Params;
						Params.AddIgnoredActors(IgnoredActorsForTrace);

						// 必须用 ECC_WorldStatic，彻底无视各种特效和物理穿透件
						if (SafeWorld->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_WorldStatic, Params))
						{
							// ==================== 【胶囊体中心高度补齐】 ====================
							// 兜底半高
							float CapsuleHalfHeight = 90.0f;

							// 直接从活着的队友身上提取真实的胶囊体高度
							if (UCapsuleComponent* Cap = Teammate->GetCapsuleComponent())
							{
								CapsuleHalfHeight = Cap->GetUnscaledCapsuleHalfHeight();
							}

							// 真实地板 Z 坐标 + 胶囊体真实半高 + 1.0f 容差 = 完美的落地中心点
							OffsetLoc.Z = GroundHit.ImpactPoint.Z + CapsuleHalfHeight + 1.0f;
						}
						else
						{
							// 兜底防错
							float CapsuleHalfHeight = 90.0f;
							if (UCapsuleComponent* Cap = Teammate->GetCapsuleComponent())
							{
								CapsuleHalfHeight = Cap->GetUnscaledCapsuleHalfHeight();
							}
							OffsetLoc.Z = BaseFloorZ + CapsuleHalfHeight + 1.0f;
						}

						// ==================== 【同图核心：直接瞬移现有肉体】 ====================
						// 参数 false：关闭 Sweep 碰撞检测，无视沿途障碍物强行瞬移。
						// 参数 true：TeleportPhysics，瞬移后保留物理属性（由于我们在前置函数已将其完全冻结，此处绝对安全）。
						// 核心纠正：服务器对所有肉体强行执行物理折叠，虚幻的 Replicated Movement 会瞬间下发客机，杜绝拉扯！
						Teammate->TeleportTo(OffsetLoc, TargetTransform.GetRotation().Rotator(), false, true);

						// 瞬移后立刻点穴，防止在黑幕期间掉落
						if (UCharacterMovementComponent* CMC = Teammate->GetCharacterMovement())
						{
							CMC->DisableMovement();
						}

						// 将刚落地成功的队友立刻加入黑名单，绝不干扰下一个人的探测
						IgnoredActorsForTrace.Add(Teammate);
						UE_LOG(LogTemp, Warning, TEXT("🚀 [服务器上帝视角] 肉体已成功分配阵型并完成物理搬运: %s"), *Teammate->GetName());
					}
				}

				// ==============================================================================
				// 【解封】：物理搬运全量完成！向所有被蒙着眼睛的客机广播下达解封令！
				// ==============================================================================
				for (FConstPlayerControllerIterator It = SafeWorld->GetPlayerControllerIterator(); It; ++It)
				{
					if (APlayerController* IterPC = It->Get())
					{
						if (UMyMapTravelNetComponent* NetComp = IterPC->FindComponentByClass<UMyMapTravelNetComponent>())
						{
							NetComp->Client_NotifyPhysicalTeleportDone();
							NetComp->ResetSyncState();
						}
					}
				}
			}
		}, 0.05f, true);
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
	// 默认的普通单机跨图漫游：走无缝漫游 (bAbsolute = false)
	InternalExecuteTravel(TargetLevelName, TargetLevelName.ToString(), false, false);
}

void UMyMapTravelSubsystem::ExecuteHostTravel(FName TargetLevelName)
{
	// 房主专属建房跳转（带队）：强制绝对跳转 (bAbsolute = true)，附加 ?listen
	FString HostURL = FString::Printf(TEXT("%s?listen"), *TargetLevelName.ToString());
	InternalExecuteTravel(TargetLevelName, HostURL, true, false);
}

void UMyMapTravelSubsystem::ExecuteClientJoin(const FString& ConnectString)
{
	// 客户端专属飞线加入：强制绝对跳转 (bAbsolute = true)
	InternalExecuteTravel(NAME_None, ConnectString, true, true);
}

void UMyMapTravelSubsystem::InternalExecuteTravel(FName TargetLevelName, const FString& TravelURL, bool bIsAbsolute, bool bIsClientJoin)
{
	// 转场状态互斥锁：防止玩家在黑屏淡出期间狂按按键或连踩触发器导致状态机重复执行
	if (bIsTraveling) return;

	// 锁定全局转场状态，宣布进入不可逆的传送管线
	bIsTraveling = true;

	// 安全获取当前世界上下文
	UWorld* World = GetWorld();

	// 终极防呆校验：如果世界不存在，或者传入的地址为空，直接中止并解锁状态
	// (注意：客户端飞线加入时 TargetLevelName 为空是合法的，所以校验 TravelURL)
	if (!IsValid(World) || TravelURL.IsEmpty())
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

	// 尝试获取全局大管家 GameInstance
	UMyGameInstance* GI = World->GetGameInstance<UMyGameInstance>();

	// ==============================================================================
	// 【新增核心：清空旧档案】
	// ==============================================================================
	if (GI)
	{
		// 清除上一局残留的主机真身数据
		GI->PlayerClassMemory.Empty();
		// 【完美闭环】：彻底清空上一局的小队缓存，防止脏数据带入新地图
		GI->SquadClassMemory.Empty();
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

			// ==============================================================================
			// 【完美还原原版防粘滞机制】 
			// 原版设计极其严谨：必须清空按键缓存，防止落地走火！
			// ==============================================================================
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

				// 跨地图踩门的一瞬间，立刻物理死锁旧肉体！防止黑幕起步时滑行或掉落
				if (ACharacter* PlayerCharacter = Cast<ACharacter>(PlayerPawn))
				{
					// 提取角色移动组件 (CMC)
					if (UCharacterMovementComponent* CMC = PlayerCharacter->GetCharacterMovement())
					{
						// 掐断重力与动力，角色被完全冻结
						CMC->DisableMovement();
					}
				}

				// ==============================================================================
				// 【跨地图打包装车】：记录玩家当前正在控制的肉体
				// ==============================================================================
				if (GI && PC->PlayerState)
				{
					// 提取网络底层唯一 ID 或本地 ID，防局域网碰撞
					FString NetId = PC->PlayerState->GetUniqueId().IsValid() ? PC->PlayerState->GetUniqueId().ToString() : FString::Printf(TEXT("LOCAL_PLAYER_%d"), PC->PlayerState->GetPlayerId());

					// 存入哈希表 (ID -> 当前角色的蓝图类)
					GI->PlayerClassMemory.Add(NetId, PlayerPawn->GetClass());
					UE_LOG(LogTemp, Warning, TEXT("📦 [大世界流送] 玩家当前真身已装车: %s -> %s"), *NetId, *PlayerPawn->GetClass()->GetName());
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

	// ==============================================================================
	// 【小队打包装车 (Squad Packing)】
	// ==============================================================================
	if (GI)
	{
		// 寻找当前世界的游戏模式，索要最权威的小队花名册
		if (AMyGameModeBase* GM = Cast<AMyGameModeBase>(World->GetAuthGameMode()))
		{
			for (ATopCharacter* Teammate : GM->FriendlyRoster)
			{
				// 防御性校验：确保实体有效且未被销毁
				if (Teammate && !Teammate->IsActorBeingDestroyed())
				{
					// 💥【终极真相核心修正】：绝对过滤：只跳过本地房主！
					// 必须把客机和 AI 一视同仁，全部作为“无主躯壳”打包带进新世界！
					if (Teammate->GetController() && Teammate->GetController()->IsLocalController())
					{
						continue;
					}

					// 此时副机和 AI 都会被装进记忆库，供服务器落地时统一排队生成
					GI->SquadClassMemory.Add(Teammate->GetClass());
					UE_LOG(LogTemp, Warning, TEXT("📦 [小队流送] 待命队友/副机躯壳已打包装车: %s"), *Teammate->GetClass()->GetName());
				}
			}
		}
	}

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

	// ==============================================================================
	// 【网络握手第一阶段：触发所有客户端拉起跨图专属黑幕】
	// ==============================================================================
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* IterPC = It->Get())
		{
			if (UMyMapTravelNetComponent* NetComp = IterPC->FindComponentByClass<UMyMapTravelNetComponent>())
			{
				// 下发跨图 RPC 握手，客机将自动拉起熄屏 UI 并返回 Ready
				NetComp->Server_InitiateCrossMapTransition(TargetLevelName);
			}
		}
	}

	// ==============================================================================
	// 【网络握手第二阶段：高频轮询死等全员闭眼 + 物理时序死锁】
	// ==============================================================================
	double StartTime = World->GetTimeSeconds();

	// 强制物理等待时间：死等设计师配置的黑屏时间走完
	double RequiredPhysicalWait = OutConfig.ScreenOffDuration;
	// 物理防死锁计算：取 UI 动画时长再加 2 秒作为极限断连超时
	double MaxTravelWait = FMath::Max(5.0, RequiredPhysicalWait + 2.0);

	// 使用 TWeakObjectPtr 包装世界指针，防止 2 秒等待期内世界被意外销毁导致野指针崩溃
	TWeakObjectPtr<UMyMapTravelSubsystem> WeakThis(this);
	TWeakObjectPtr<UWorld> WeakWorld(World);

	// 开启高频定时器，死等网络确认与物理时间流逝完毕
	World->GetTimerManager().SetTimer(SyncWaitTimerHandle, [WeakThis, WeakWorld, TravelURL, bIsAbsolute, bIsClientJoin, StartTime, RequiredPhysicalWait, MaxTravelWait]()
		{
			// 唤醒时重新安全提取强引用，如果内存已经被销毁，这里会安全返回 nullptr
			UMyMapTravelSubsystem* StrongThis = WeakThis.Get();
			UWorld* SafeWorld = WeakWorld.Get();
			if (!StrongThis || !SafeWorld) return;

			double ElapsedTime = SafeWorld->GetTimeSeconds() - StartTime;

			// 💥 【核心时序防线】：物理时间必须流逝完毕！绝不让肉体在闭眼前提前消失！
			bool bTimeElapsed = ElapsedTime >= RequiredPhysicalWait;
			bool bAllReady = true;

			// 严密监控当前局内所有客机是否已经拉好黑幕
			for (FConstPlayerControllerIterator It = SafeWorld->GetPlayerControllerIterator(); It; ++It)
			{
				if (APlayerController* PC = It->Get())
				{
					if (UMyMapTravelNetComponent* NetComp = PC->FindComponentByClass<UMyMapTravelNetComponent>())
					{
						if (NetComp->GetSyncState() != ETravelSyncState::Ready)
						{
							bAllReady = false;
							break;
						}
					}
				}
			}

			// 超时强制放行防线：防止有客机彻底掉线导致全服死锁
			if (ElapsedTime >= MaxTravelWait)
			{
				bAllReady = true;
				bTimeElapsed = true;
			}

			// ==============================================================================
			// 【网络握手第三阶段：网络与物理双重就绪，启动底层无缝跨界！】
			// ==============================================================================
			// 必须满足：所有客机收到黑幕指令 (Ready) AND 物理黑幕时间已走完 (TimeElapsed)
			if (bAllReady && bTimeElapsed)
			{
				// ==============================================================================
				// 💥 【终极排雷】：打破 C++ Lambda 的悬空指针魔咒！
				// 由于紧接着要调用 ClearTimer 抹杀本定时器，会导致本 Lambda 内捕获的所有堆内存变量全部被物理销毁！
				// 必须在“自杀”前，把接下来需要用到的参数全部深拷贝到栈内存中！
				// ==============================================================================
				FString SafeTravelURL = TravelURL;
				bool bSafeIsAbsolute = bIsAbsolute;
				bool bSafeIsClientJoin = bIsClientJoin;

				// 满足条件，立刻销毁轮询器
				SafeWorld->GetTimerManager().ClearTimer(StrongThis->SyncWaitTimerHandle);

				// 时序归位：真正起航！
				if (bSafeIsClientJoin)
				{
					if (APlayerController* PC = SafeWorld->GetFirstPlayerController())
					{
						PC->ClientTravel(SafeTravelURL, TRAVEL_Absolute);
					}
				}
				else
				{
					if (bSafeIsAbsolute)
					{
						// 房主建房必须剥夺旧世界的无缝漫游防截断
						if (AGameModeBase* CurrentGM = SafeWorld->GetAuthGameMode())
						{
							CurrentGM->bUseSeamlessTravel = false;
						}
					}
					// 命令引擎底层开启旅行管线，跨越位面！告别乱码 ⹂Ɍ！
					SafeWorld->ServerTravel(SafeTravelURL, bSafeIsAbsolute);
				}
			}
		}, 0.05f, true);
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

		// 1. 清理黑幕期间玩家盲按留下的常规物理按键状态
		PC->FlushPressedKeys();

		// 2. 针对 UE 5.8 增强输入：强制玩家松开按键后才接受新输入，彻底杜绝落地自动开火/走位的粘滞 Bug
		if (UEnhancedInputLocalPlayerSubsystem* EISub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			FModifyContextOptions Options;
			Options.bIgnoreAllPressedKeysUntilRelease = true;
			EISub->RequestRebuildControlMappings(Options, EInputMappingRebuildType::RebuildWithFlush);
		}

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

		// ==============================================================================
		// 【致命遗漏修复：全量唤醒小队队友 (Squad Unfreeze)】
		// 之前在同图传送时，由于只唤醒了玩家真身，队友被 DisableMovement 永久定格在了空中！
		// 在这里必须向全队下达解穴令，让底层的 FindFloor 瞬间完成贴地吸附！
		// ==============================================================================
		if (AMyGameModeBase* GM = Cast<AMyGameModeBase>(World->GetAuthGameMode()))
		{
			for (ATopCharacter* Teammate : GM->FriendlyRoster)
			{
				if (Teammate)
				{
					if (UCharacterMovementComponent* CMC = Teammate->GetCharacterMovement())
					{
						// 解除之前的物理冻结，交还给引擎重力系统
						CMC->SetMovementMode(MOVE_Walking);
						UE_LOG(LogTemp, Warning, TEXT("🔓 [物理解穴] 队友重力与行走已恢复: %s"), *Teammate->GetName());
					}
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
	UMyGameInstance* GI = World->GetGameInstance<UMyGameInstance>();
	if (!GI) return;

	// 声明目标接机点指针，准备进行全图搜索
	AMyUniversalDestination* TargetDest = nullptr;

	// ==============================================================================
	// 【大一统闭环】：绝对的数据驱动寻址
	// ==============================================================================
	// 只有当大管家手中确实持有跨界车票 (PendingTravelRoute) 时，才去寻找传送门出口！
	if (GI->PendingTravelRoute)
	{
		// 此时新世界的 Persistent Level 已 Ready，目标必然在内存中，执行全局 Actor 扫盘
		for (TActorIterator<AMyUniversalDestination> It(World); It; ++It)
		{
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
	}

	// 核心安全获取：提取绝对真实的本地玩家控制器，防死无缝漫游产生的幽灵假身
	APlayerController* PC = GetRealPlayerController(World);
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;

	// 记录小队落地的基准坐标
	FVector BaseLocation = FVector::ZeroVector;
	FRotator BaseRotation = FRotator::ZeroRotator;

	// 全局锁定落地基准点
	if (TargetDest && Pawn)
	{
		// 提取目标点的绝对坐标，Z轴强制增加 15cm 的高度冗余，作为完美防穿模的下落空间
		BaseLocation = TargetDest->GetActorLocation() + FVector(0.0f, 0.0f, 15.0f);
		BaseRotation = TargetDest->GetActorRotation();

		if (GI->PendingTravelRoute)
		{
			UE_LOG(LogTemp, Warning, TEXT("[MapTravelLog] 💥 瞬移成功！玩家真身强行穿插至目标路由: %s"), *GI->PendingTravelRoute->GetName());
		}
	}
	else if (Pawn)
	{
		// 🎯 兜底：如果没有找到接机点（场景里没放，或者策划漏配了）
		// 绝对不能跳过小队的生成逻辑！就以玩家当前现有的兜底位置为基准
		BaseLocation = Pawn->GetActorLocation();
		BaseRotation = Pawn->GetActorRotation();

		UE_LOG(LogTemp, Warning, TEXT("[MapTravelLog] ⚠️ 警告：未找到路由对应的接机点，采用默认出生点兜底。"));
	}

	// 🚀 逻辑拆分点 1：处理本地表现 (仅在 LocalPlayer 所在的机器执行)
	if (PC && PC->IsLocalController())
	{
		HandleLocalPlayerSnapping(World, TargetDest);
	}

	// 🚀 逻辑拆分点 2：处理权威部署 (单机开荒、Listen Server 主机执行)
	// 这个条件已经天然剔除了客机，因此 DeploySquadTeammates 内部只需要走最纯粹的生成逻辑
	if (World->GetNetMode() < NM_Client)
	{
		DeploySquadTeammates(World, TargetDest, GI);
	}

	// ==============================================================================
	// 【联机时序加固】：已彻底剥夺房主在此处的“私自撕票权”！
	// 原先的 GI->PendingTravelRoute = nullptr; 已被删除。
	// 必须保留车票，直到 GameMode 的 StartPlay (NextTick) 确认所有首批客机都已安全连入，
	// 再由大管家统一销毁，彻底根绝客机网卡导致查票失败、掉回出生点的隐患。
	// ==============================================================================
}

void UMyMapTravelSubsystem::HandleLocalPlayerSnapping(UWorld* World, AMyUniversalDestination* Dest)
{
	// 核心安全获取：提取绝对真实的本地玩家控制器，防死无缝漫游产生的幽灵假身
	APlayerController* PC = GetRealPlayerController(World);
	if (!PC || !PC->IsLocalController()) return;

	// ==============================================================================
	// 【新框架大一统：本地物理剥夺】
	// 既然已经全面拥抱“服务器上帝视角排队”的绝对权威，本地（无论主机还是客机）
	// 都绝不再执行任何 TeleportTo 物理操作！将物理权 100% 移交给 DeploySquadTeammates！
	// 此函数退化为纯粹的表现层：只负责加载美术资源与修正镜头。
	// ==============================================================================

	if (Dest)
	{
		// 1. 本地表现层预热：跨图落地时，立即刷新数据层滑动窗口
		if (Dest->BoundDataLayer)
		{
			UE_LOG(LogTemp, Warning, TEXT("[MapTravelLog] 🔄 跨图落地表现层：立刻刷新目标点的数据层滑动窗口"));
			if (UMyDataLayerStreamingSubsystem* StreamingSub = World->GetSubsystem<UMyDataLayerStreamingSubsystem>())
			{
				StreamingSub->RefreshSlidingWindow(Dest->BoundDataLayer, true);
			}
		}

		// 2. 镜头视口对齐：强行把玩家控制器的相机视角拧过去，防止镜头滞后导致的平移拖影！
		// 摄像机旋转不涉及物理引擎碰撞，本地执行绝对安全
		PC->SetControlRotation(Dest->GetActorRotation());
	}

	UE_LOG(LogTemp, Warning, TEXT("[MapTravelLog] 👁️ 本地表现层就绪！跳过所有物理探针，静默等待服务器排队分配肉体坐标！"));
}

void UMyMapTravelSubsystem::DeploySquadTeammates(UWorld* World, AMyUniversalDestination* Dest, UMyGameInstance* GI)
{
	APlayerController* PC = GetRealPlayerController(World);
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;

	// 记录小队落地的基准坐标
	FVector BaseLocation = FVector::ZeroVector;
	FRotator BaseRotation = FRotator::ZeroRotator;

	if (Dest)
	{
		// 提取目标点的绝对坐标，Z轴强制增加 15cm 的高度冗余，作为完美防穿模的下落空间
		BaseLocation = Dest->GetActorLocation() + FVector(0.0f, 0.0f, 15.0f);
		BaseRotation = Dest->GetActorRotation();
	}
	else if (Pawn)
	{
		// 🎯 修复：如果没有找到接机点（场景里没放，或者策划漏配了）
		// 绝对不能跳过小队的生成逻辑！就以玩家当前现有的兜底位置为基准
		BaseLocation = Pawn->GetActorLocation();
		BaseRotation = Pawn->GetActorRotation();
	}
	else
	{
		// 终极兜底：主机都没生出来时，找地图原配起点
		if (AActor* PS = UGameplayStatics::GetActorOfClass(World, APlayerStart::StaticClass()))
		{
			BaseLocation = PS->GetActorLocation();
			BaseRotation = PS->GetActorRotation();
		}
	}

	// ==============================================================================
	// 【全量角色屏蔽 (Global Blacklist)】
	// ==============================================================================
	TArray<AActor*> IgnoredActorsForTrace;
	for (TActorIterator<ATopCharacter> It(World); It; ++It)
	{
		if (ATopCharacter* Char = *It)
		{
			IgnoredActorsForTrace.Add(Char);
		}
	}

	// ==============================================================================
	// 【新框架大一统：跨图废弃 PlayerStart 挪位，全面拥抱上帝视角统筹】
	// ==============================================================================

	// 第一步：只管把跨图记忆库里的 AI 躯壳释放出来
	for (TSubclassOf<ATopCharacter> TeammateClass : GI->SquadClassMemory)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		// 随便在基准点上方生出来即可，它的 BeginPlay 会自动将其注册进 GM->FriendlyRoster
		World->SpawnActor<ATopCharacter>(TeammateClass, BaseLocation + FVector(0, 0, 500.0f), BaseRotation, SpawnParams);
	}

	// 🚀 内存优化：落地结束，立刻清空小队数组，释放类引用防止 TArray 内存泄漏！
	GI->SquadClassMemory.Empty();
	UE_LOG(LogTemp, Warning, TEXT("🧹 [内存优化] 小队记忆库已释放，等待下一轮装车。"));

	// ==============================================================================
	// 第二步：复用同地图的“上帝视角统一排队”逻辑！
	// 不管是刚连入生在十万八千里的副机，还是刚才生在半空中的 AI，统统强行抓过来排队！
	// ==============================================================================
	if (AMyGameModeBase* GM = Cast<AMyGameModeBase>(World->GetAuthGameMode()))
	{
		float BaseFloorZ = BaseLocation.Z;
		float CurrentXOffset = 0.0f; // 0 点保留给第一顺位（通常是房主）
		bool bFlipOffset = true;

		for (ATopCharacter* Teammate : GM->FriendlyRoster)
		{
			// 基础安全校验
			if (!Teammate || Teammate->IsActorBeingDestroyed()) continue;

			// 左右交替偏移：基于基准点
			FVector OffsetLoc = BaseLocation;
			OffsetLoc.X += (bFlipOffset ? CurrentXOffset : -CurrentXOffset);

			// 每排完一左一右，间距拉大
			if (!bFlipOffset || CurrentXOffset == 0.0f)
			{
				CurrentXOffset += 120.0f;
			}
			if (CurrentXOffset > 0.0f)
			{
				bFlipOffset = !bFlipOffset;
			}

			// 贴地短探针，寻找真实地板
			FHitResult GroundHit;
			FVector TraceStart = OffsetLoc + FVector(0, 0, 150.0f);
			FVector TraceEnd = OffsetLoc - FVector(0, 0, 300.0f);

			FCollisionQueryParams Params;
			Params.AddIgnoredActors(IgnoredActorsForTrace);

			// 统一物理标准：使用 ECC_WorldStatic 绝对地形探针
			if (World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_WorldStatic, Params))
			{
				float CapsuleHalfHeight = 90.0f;
				if (UCapsuleComponent* Cap = Teammate->GetCapsuleComponent())
				{
					CapsuleHalfHeight = Cap->GetUnscaledCapsuleHalfHeight();
				}
				// 真实地板 Z 坐标 + 胶囊体真实半高 + 1.0f 容差 = 完美的落地中心点
				OffsetLoc.Z = GroundHit.ImpactPoint.Z + CapsuleHalfHeight + 1.0f;
			}
			else
			{
				// 兜底防错
				float CapsuleHalfHeight = 90.0f;
				if (UCapsuleComponent* Cap = Teammate->GetCapsuleComponent())
				{
					CapsuleHalfHeight = Cap->GetUnscaledCapsuleHalfHeight();
				}
				OffsetLoc.Z = BaseFloorZ + CapsuleHalfHeight + 1.0f;
			}

			// 核心暗箱：强行将所有名册里的躯壳折叠至计算好的完美阵型！
			// 底层 Network Replicated Movement 会瞬间剥夺客机的物理反抗权，杜绝拉扯弹射！
			Teammate->TeleportTo(OffsetLoc, BaseRotation, false, true);

			// 物理点穴：在黑屏彻底解开前，锁死全队重力坠落，保证完美滞空等UI开幕
			if (UCharacterMovementComponent* CMC = Teammate->GetCharacterMovement())
			{
				CMC->DisableMovement();
			}

			// 将刚落地成功的队友立刻加入黑名单，绝不干扰下一个人的探测
			IgnoredActorsForTrace.Add(Teammate);
			UE_LOG(LogTemp, Warning, TEXT("🚀 [跨图上帝视角] 肉体已成功分配阵型并完成物理搬运: %s"), *Teammate->GetName());
		}
	}
}

#pragma endregion
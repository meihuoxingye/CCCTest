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
#include "GameFramework/Character.h" 
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
#include "GameFramework/PlayerStart.h"
#include "World/MyMapAttributeDataAsset.h"

// 【核心新增】：引入跨图与流送专属玩家状态令牌契约
#include "PlayerState/MyPlayerState.h"
#include "PlayerState/Component/TravelAndStreaming/MyMapTravelStateComponent.h"

#include "UI/Transition/MyScreenOffWidget.h"

// 引入全局公共状态板
#include "Game/MyGameStateBase.h"


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

	// 💥💥💥 彻底移除了原有的 bIsPhysicalLayoutReady = false; 💥💥💥
	// 绝对禁止在这里重置地基锁！开荒夺舍比这里执行得更早，绝不能把建好的锁砸烂。

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
// 令牌响应管线 (Token Response Pipeline) - 彻底消灭客户端越权
// ==============================================================================
#pragma region

void UMyMapTravelSubsystem::HandleDeploymentTokenUpdate(UMyMapTravelStateComponent* PS, ETravelDeploymentStatus OldStatus, int32 RetryCount)
{
	// 安全校验：防空指针
	if (!PS) return;

	// 安全校验：从组件向上找宿主 (PlayerState)
	AMyPlayerState* OwningPS = Cast<AMyPlayerState>(PS->GetOwner());
	if (!OwningPS) return;

	UMyGameInstance* GI = GetWorld()->GetGameInstance<UMyGameInstance>();
	if (!GI) return;

	// ==============================================================================
	// 💥【修复：保存入场状态快照，防止被后续代码中途篡改】
	// 
	// 为什么不能在最底下的 switch 中直接写 switch (PS->DeploymentStatus) ？
	// 请看下面这个极度危险的函数嵌套调用顺序：
	// 1. 本次函数被触发，玩家带着【WaitingForShell】的状态进来了。
	// 2. 代码往下走，调用了下方的物理总闸 CheckAndExecutePhysicalDeployment()。
	// 3. 总闸在里面把肉体分配好了，并且直接在内存里把玩家的状态改成了【ReadyToPossess】。
	// 4. 总闸执行完毕，代码退回到这里，继续往下走，来到了最底部的 switch 语句。
	// 5. 如果此时 switch 去读 PS->DeploymentStatus，它读到的会是刚刚被改写的【ReadyToPossess】！
	// 6. 结果：本次函数原本是为了处理 WaitingForShell，却阴差阳错地跑进了 ReadyToPossess 的分支，导致逻辑错乱（出现双重打印）。
	// 
	// 【解法】：在函数第一行，立刻把玩家刚进门时的状态“复印”一份 (SnapshotStatus)。
	// 无论中间的深层函数怎么改写内存里的状态，我们接下来的代码只认这份复印件！
	// ==============================================================================
	ETravelDeploymentStatus SnapshotStatus = PS->DeploymentStatus;

	// ==============================================================================
	// 👑【服务器权威管线】：绝对不能被下方查找 LocalPC 的表现层逻辑阻塞！
	// 服务器必须第一时间无条件处理所有副机的 Ack，否则大一统总闸必定死锁！
	// ==============================================================================
	if (OwningPS->HasAuthority())
	{
		// ==============================================================================
		// 💥【架构一致性：服务器端快照核验】
		// 为什么服务器端也必须使用 SnapshotStatus 而不是 PS->DeploymentStatus？
		// 因为这段服务器代码，恰恰就是触发“状态中途被篡改”的作案源头！
		// 如果这里不使用快照，一旦内部的 CheckAndExecutePhysicalDeployment 成功发牌解封，
		// 那么在同一个函数的上下文中，物理概念上的“当前状态”就发生了撕裂。
		// 统一使用快照，确保“一次事件回调，只处理一个绝对的历史瞬间”，坚决贯彻 C++ 的决定论原则！
		// ==============================================================================
		if (SnapshotStatus == ETravelDeploymentStatus::WaitingForShell)
		{
			if (!bIsPhysicalLayoutReady)
			{
				// 情况 A：全局物理地基未就绪 (大部队集体跨图/同图中)。
				// 交给大一统总闸统筹，所有人必须死等，凑齐了一起走！
				CheckAndExecutePhysicalDeployment();
			}
			else
			{
				// ==============================================================================
				// 💥【核心修复：副机中途加入专属管线】
				// 情况 B：全局地基早已就绪 (房主在正常游玩)，此时收到 Ack，说明是客机中途连入加载完毕！
				// 它不需要等总闸！直接单独为其验明正身，发牌解封！
				// ==============================================================================
				UE_LOG(LogTemp, Warning, TEXT("🚪 [中途加入管线] 侦测到副机连入！全局地基已就绪，执行专属客机发牌！"));

				APlayerController* TargetPC = nullptr;
				for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
				{
					if (It->Get()->PlayerState == OwningPS)
					{
						TargetPC = It->Get();
						break;
					}
				}

				if (TargetPC)
				{
					if (!TargetPC->GetPawn())
					{
						// 兜底：如果 GameMode 还没来得及给它发肉体，强行走一次大名单躯壳分配
						if (AMyGameModeBase* GM = Cast<AMyGameModeBase>(GetWorld()->GetAuthGameMode()))
						{
							GM->RestartPlayer(TargetPC);
						}
					}
					else
					{
						// 完美闭环：肉体已分配，直接单独下发解封令牌，该副机的 UI 瞬间解穴！
						PS->SetDeploymentStatus(ETravelDeploymentStatus::ReadyToPossess);
					}
				}
			}
		}
	}

	// ==============================================================================
	// 📺【本地表现层管线】：只有本地真实玩家才允许处理表现层和物理输入解封
	// 💥 核心修复：彻底抛弃玄学的 GetOwner()，直接遍历本地控制器查表匹配！绝不漏掉 UI 指令！
	// ==============================================================================
	APlayerController* LocalPC = nullptr;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			// 如果是远端玩家的代理，IsLocalController 会返回 False，直接过滤
			if (PC->PlayerState == OwningPS && PC->IsLocalController())
			{
				LocalPC = PC;
				break;
			}
		}
	}

	// ==============================================================================
	// 💥 终极联机修复：解决客机中途加入时的网络数据竞争 (Race Condition)！
	// 客机初次加载时，PlayerState 的属性 (Token) 往往先于 PlayerController 的指针绑定完成复制！
	// 此时如果找不到 LocalPC，挂起 0.05 秒后递归轮询，死等指针绑定完成！
	// ==============================================================================
	if (!LocalPC)
	{
		// 【防幽灵锁】：只有纯客户端才需要死等绑定，主机端找不到直接当做客机代理扔掉！
		if (GetWorld()->GetNetMode() == NM_Client)
		{
			// 增加熔断机制：最多重试 50 次 (约 2.5 秒)
			const int32 MAX_RETRY = 50;
			if (RetryCount >= MAX_RETRY)
			{
				// 💥 【双轨并行挂起】：2.5秒高频轮询结束，安全挂起。
				UE_LOG(LogTemp, Verbose, TEXT("🛡️ [令牌拦截/挂起] 轮询结束：若为队友则静默丢弃；若为本地极度卡顿，等待 OnRep_PlayerState 被动唤醒。"));
				return;
			}

			FTimerHandle RetryTimer;
			TWeakObjectPtr<UMyMapTravelSubsystem> WeakThis(this);
			TWeakObjectPtr<UMyMapTravelStateComponent> WeakPS(PS);

			GetWorld()->GetTimerManager().SetTimer(RetryTimer, [WeakThis, WeakPS, OldStatus, RetryCount]()
				{
					if (UMyMapTravelSubsystem* StrongThis = WeakThis.Get())
					{
						if (UMyMapTravelStateComponent* ValidPS = WeakPS.Get())
						{
							StrongThis->HandleDeploymentTokenUpdate(ValidPS, OldStatus, RetryCount + 1);
						}
					}
				}, 0.05f, false);
		}

		// 不是 LocalPC 绝对不能往下执行，直接拦截
		return;
	}

	// ==============================================================================
	// 💥 严禁读取内存里的最新状态 ! 必须使用第一行的复印件（快照）进行本地响应！
	// ==============================================================================
	switch (SnapshotStatus)
	{
	case ETravelDeploymentStatus::Traveling:
	{
		// 收到 Traveling 令牌，客户端丧失一切权力，强行拉起黑幕
		UE_LOG(LogTemp, Warning, TEXT("🎫 [令牌契约] 收到 Traveling 指令，客户端立刻移交控制权，拉起黑幕！"));

		// 【新增】：无论同图跨图，起航时重置三把锁
		GI->ResetTransitionLocks();

		FName CurrentMapName = FName(*UGameplayStatics::GetCurrentLevelName(GetWorld(), true));
		FMapTransitionConfig Config = GI->GetMapTransitionConfig(CurrentMapName);

		// 直接触发大管家的闭眼操作，代替以前被删掉的 Client_HandleTransitionRequest
		GI->PlayScreenOffPhaseUI(Config.ScreenOffUIClass, Config.ScreenOffDuration);
		break;
	}

	case ETravelDeploymentStatus::WaitingForShell:
	{
		// ==============================================================================
		// 💥【架构解耦】：UI 的拉起和发信号已经交由本地大管家抢跑处理！
		// 此处客户端唯一要做的就是挂起，安心等待服务器物理搬运完成发牌。
		// ==============================================================================
		UE_LOG(LogTemp, Warning, TEXT("🛑 [令牌契约] 收到 WaitingForShell！(物理流送进行中，UI掩护已由大管家本地接管)"));
		break;
	}

	case ETravelDeploymentStatus::ReadyToPossess:
	{
		// 肉体阵型已就绪，立刻交还表现权
		UE_LOG(LogTemp, Warning, TEXT("🟢 [令牌契约] 收到 ReadyToPossess！肉体地块就绪，三锁判定退场！"));

		// 跨图/同图共用：向大管家通报第三把锁 (服务器物理阵型) 已就绪！
		GI->NotifyPhysicalReady();
		break;
	}

	default:
		break;
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



void UMyMapTravelSubsystem::NotifyLocalScreenOffFinished()
{
	// 提取绝对真实的本地玩家控制器
	if (APlayerController* PC = GetRealPlayerController(GetWorld()))
	{
		// O(1) 极速提取成员变量
		if (AMyPlayerState* MyPS = PC->GetPlayerState<AMyPlayerState>())
		{
			if (UMyMapTravelStateComponent* NetComp = MyPS->MapTravelComponent)
			{
				// ==============================================================================
				// 💥【核心修复：状态机单向防反转锁】
				// 只有在没就绪 (如 Traveling 或 刚连入) 时，才允许通过 Ack 切换为 WaitingForShell。
				// 如果玩家早就已经是 ReadyToPossess (例如冷启动开荒早在 RestartPlayer 就已完成附身夺舍)，
				// 绝不允许迟来的 UI 遮罩信号将其状态【降级】打回 WaitingForShell！
				// ==============================================================================
				if (NetComp->DeploymentStatus != ETravelDeploymentStatus::ReadyToPossess)
				{
					UE_LOG(LogTemp, Warning, TEXT("📺 [表现层] 熄屏 UI 彻底闭合黑透！向服务器正式发射就绪 Ack！"));
					NetComp->Server_AckScreenOffReady();
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("📺 [表现层] 熄屏 UI 掩护就绪，但玩家已处于 ReadyToPossess 状态，静默忽略 Ack 以防状态倒车。"));
				}
			}
		}
	}
}

void UMyMapTravelSubsystem::ExecuteUniversalTravel(AActor* TeleportingActor, UTeleportRoute* TargetRoute)
{
	// 基础防线：拦截无效的传送物理实体或未配置的空路由资产
	if (!TeleportingActor || !TargetRoute) return;

	// 打印大一统传送系统的入口分割线与寻址起点日志
	UE_LOG(LogTemp, Warning, TEXT("[MapTravelLog] ==============================================="));
	UE_LOG(LogTemp, Warning, TEXT("[MapTravelLog] 🚀 玩家踩上传送门！开始大一统路由寻址。收到的目标路线: [%s]"), *TargetRoute->GetName());

	UWorld* World = GetWorld();
	if (!World) return;

	// ==============================================================================
	// 💥 【核心修正：大一统寻址基准】
	// 绝不能用本地字典 (SameMapDestinationRegistry) 来判断是否同图！
	// 因为如果策划把接机点放在了还没流送的数据层(DataLayer)里，字典里根本没有它！
	// 必须直接提取当前世界的纯净地图名，与车票上的目标地图名进行绝对字符串比对！
	// ==============================================================================
	FName CurrentMapName = FName(*UGameplayStatics::GetCurrentLevelName(World, true));
	FString TargetMapString = TargetRoute->TargetMap.GetAssetName();
	FName TargetMapName = FName(*TargetMapString);

	if (TargetMapName == CurrentMapName)
	{
		// 只要名字一样，哪怕字典里现在查不到接机点，也必须按【同图漫游】处理！
		// 找不到接机点的异常，将全权交由 ExecuteSameMapTravel 内部的【两级物理兜底】去抢救！
		UE_LOG(LogTemp, Warning, TEXT("[MapTravelLog] -> 🔍 路由比对成功：目标属于当前世界，判定为【同地图传送】！"));
		ExecuteSameMapTravel(TeleportingActor, TargetRoute);

		// 寻址完成，直接阻断后续跨图逻辑
		return;
	}

	// 名字不一样，说明要去新世界，断定为跨界航行
	UE_LOG(LogTemp, Warning, TEXT("[MapTravelLog] -> 🔍 路由比对失败：目标属于其他世界，判定为【跨地图漫游】！"));

	// 提取全局大管家，准备移交跨地图流送指令
	if (UMyGameInstance* GI = World->GetGameInstance<UMyGameInstance>())
	{
		// 校验路由资产内部是否确实配置了跨图的目的地
		if (!TargetRoute->TargetMap.IsNull())
		{
			// 铸造跨界车票并交由全局大管家保管，留待落地后验证接机点
			GI->PendingTravelRoute = TargetRoute;

			// 输出跨界车票发放成功的日志
			UE_LOG(LogTemp, Warning, TEXT("[MapTravelLog] -> 🎫 成功购买跨图车票: [%s]，准备前往新世界: [%s]"), *TargetRoute->GetName(), *TargetMapName.ToString());

			// 正式启动底层跨地图传送流送管线
			ExecuteMapTravel(*TargetMapName.ToString());
		}
		else
		{
			// 策划配置失误：既不是同图，也没填跨图关卡名，抛出致命错误日志
			UE_LOG(LogTemp, Error, TEXT("[大一统传送系统] 寻路瘫痪！路由资产 [%s] 未配置目标跨界地图！"), *TargetRoute->GetName());
		}
	}
}

void UMyMapTravelSubsystem::ExecuteSameMapTravel(AActor* TeleportingActor, UTeleportRoute* TargetRoute)
{
	// 时序锁与合法性首重校验：防玩家连踩触发器导致状态机重入，并确保传入参数绝不为空
	if (bIsTraveling || !TeleportingActor || !TargetRoute) return;

	// 锁定全局转场状态，宣布进入不可逆的传送管线
	bIsTraveling = true;
	bIsPhysicalLayoutReady = false; // 重置地基锁

	// 安全提取世界上下文
	UWorld* World = GetWorld();
	if (!World)
	{
		// 极端情况下的逃生舱：如果世界失效，必须解开转场锁防止死锁
		bIsTraveling = false;
		return;
	}

	// 💥 大一统核心：把同图的车票也挂载到大管家上，让后方的物理总闸能查到字典！
	if (UMyGameInstance* GI = World->GetGameInstance<UMyGameInstance>())
	{
		GI->PendingTravelRoute = TargetRoute;
	}

	// ==============================================================================
	// 💥 恢复：同图专属预热机制！趁现在还没黑透，让引擎后台静默把美术资源读进内存！
	// ==============================================================================
	if (SameMapDestinationRegistry.Contains(TargetRoute))
	{
		UDataLayerAsset* TargetDataLayer = SameMapDestinationRegistry[TargetRoute].BoundDataLayer;
		if (TargetDataLayer)
		{
			UE_LOG(LogTemp, Warning, TEXT("[MapTravelLog] 🔄 同图传送：提前触发数据层后台预热"));
			if (UMyDataLayerStreamingSubsystem* StreamingSub = World->GetSubsystem<UMyDataLayerStreamingSubsystem>())
			{
				StreamingSub->PreheatZoneBackground(TargetDataLayer);
			}
			// 存入挂起池，等待 WaitingForShell 令牌到来时执行真实斩杀！
			PendingSameMapDataLayer = TargetDataLayer;
		}
	}

	// 物理层静默：强制抹平全局时空流速，为接下来的点穴做准备
	UGameplayStatics::SetGlobalTimeDilation(World, 1.0f);

	// ==============================================================================
	// 【防御性架构实装补全：全军同步点穴】
	// 在触发黑幕和网络等待前，立刻在物理世界封印全队（包括所有 AI 队友）的运动状态！
	// 绝对防止在等待数据层预热和客机 UI 的这几秒钟里，队友因重力坠落或遭受物理推挤产生位移。
	// ==============================================================================
	// 💥【修改说明】架构迁移：GameMode 的私有小本子已销毁，现在必须向 GameState 索要存活大名单
	// 才能对所有存活队友执行物理点穴。
	if (AMyGameStateBase* GS = World->GetGameState<AMyGameStateBase>())
	{
		for (ATopCharacter* Teammate : GS->FriendlyRoster)
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

	// ==============================================================================
	// 【服务器独裁第一阶段：强塞 Traveling 令牌，拉起黑幕！】
	// (完美替代了之前的 MyMapTravelNetComponent 网络组件广播)
	// ==============================================================================
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* IterPC = It->Get())
		{
			// 剥夺本地手动控制，直接通过令牌强杀
			// O(1) 极速提取成员变量
			if (AMyPlayerState* MyPS = IterPC->GetPlayerState<AMyPlayerState>())
			{
				if (UMyMapTravelStateComponent* NetPS = MyPS->MapTravelComponent)
				{
					NetPS->SetDeploymentStatus(ETravelDeploymentStatus::Traveling);
				}
			}
		}
	}

	// 💥 所有的 Timer 死等与寻址计算已物理清除，全部移交至 CheckAndExecutePhysicalDeployment 统一处理！
}

void UMyMapTravelSubsystem::CheckAndExecutePhysicalDeployment()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client) return; // 绝对的服务器物理霸权
	if (bIsPhysicalLayoutReady) return; // 拦截锁：防重复触发

	UMyGameInstance* GI = World->GetGameInstance<UMyGameInstance>();
	if (!GI) return;

	// 核验 1：所有已连接的客机是否都已经交回了 Ack (状态 >= WaitingForShell)
	bool bAllClientsReady = true;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			if (AMyPlayerState* MyPS = PC->GetPlayerState<AMyPlayerState>())
			{
				// 只要还有人在 Traveling (黑屏没播完，或者 Ack 还在天上飞)，立刻否决退回死等！
				if (MyPS->MapTravelComponent && MyPS->MapTravelComponent->DeploymentStatus == ETravelDeploymentStatus::Traveling)
				{
					bAllClientsReady = false;
					break;
				}
			}
		}
	}

	// 核验 2：引擎底层是否 Ready（跨图需等 Persistent 进内存，同图不切图必定为 Ready）
	bool bEngineReady = GI->PendingTargetMapName.IsNone() || GI->IsEngineReady();

	// 💥 并发锁核查：全员 UI 遮挡信号确认 + 引擎 Ready
	if (bAllClientsReady && bEngineReady)
	{
		UE_LOG(LogTemp, Warning, TEXT("👑 [大一统物理总闸] 收到全员 WaitingForShell 信号，令牌正式接管并执行物理流送与阵型排布！"));

		// 统一执行物理流送与排兵布阵！(内部兼容了同图切 DataLayer 和跨图找肉体)
		SnapPlayerToDestination();

		// 物理世界 100% 安全就绪，全员翻转令牌为 ReadyToPossess！
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (APlayerController* PC = It->Get())
			{
				if (AMyPlayerState* MyPS = PC->GetPlayerState<AMyPlayerState>())
				{
					if (UMyMapTravelStateComponent* NetPS = MyPS->MapTravelComponent)
					{
						// ==============================================================================
						// 💥【修复与释疑】：防重复发牌与状态机踩踏。如果 Director 内部已经为其发了解封令牌，这里直接跳过
						// 为什么此时 Director 内部已经发过牌了？这是由严密的决定论管线保证的：
						// 
						// 前置流转：刚才执行的 SnapPlayerToDestination() 内部会遍历所有处于 WaitingForShell 的玩家，并调用 RestartPlayer。
						// 
						// 场景 A【开荒】：SnapPlayerToDestination 识别到开荒锁，执行原生躯壳夺舍。夺舍成功后，底层的 ExecuteDeploymentDirector 会立刻下发 ReadyToPossess 令牌。
						// 场景 B【非开荒/跨图】：SnapPlayerToDestination 提取坐标，执行动态捏人或排队。捏人/接管成功后，底层的 ExecuteDeploymentDirector 同样会立刻下发 ReadyToPossess 令牌。
						// 
						// 结论：只要玩家在这个周期内成功分配到了肉体，当代码执行流从 SnapPlayerToDestination() 退出并走到这里时，
						// 玩家的令牌【100% 必然】已经被底层的导演系统翻转为 ReadyToPossess！
						// 如果这里不加 != ReadyToPossess 的判断直接无脑 Set，就会对同一个玩家触发双重解封信号，导致 UI 状态机重入与渲染踩踏！
						// ==============================================================================
						if (NetPS->DeploymentStatus != ETravelDeploymentStatus::ReadyToPossess)
						{
							// 下发解封令牌，触发三锁合一！
							NetPS->SetDeploymentStatus(ETravelDeploymentStatus::ReadyToPossess);
						}
					}
				}
			}
		}
	}
}

void UMyMapTravelSubsystem::CommitSameMapDataLayer()
{
	// 💥 调用时机已改变：当 WaitingForShell 令牌大一统总闸触发时，在此执行数据层斩杀
	if (PendingSameMapDataLayer)
	{
		// 输出斩杀与替换确认日志
		UE_LOG(LogTemp, Warning, TEXT("[MapTravelLog] 🔲 UI 掩护已就位！执行同图数据层真实切换与远端卸载"));

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
	// 💥【核心修复】：采纳完美方案！在加入大厅发车时，赋予客机初次添加标识！
	if (UWorld* World = GetWorld())
	{
		if (UMyGameInstance* GI = World->GetGameInstance<UMyGameInstance>())
		{
			GI->bIsClientInitialJoin = true;
		}
	}

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
		// 💥【修改说明】架构迁移：服务器 GameMode 已不再直接存储名单。
		// 现改为直接从 GameState 上的公共大屏幕提取所有幸存队友进行打包跨图流送。
		if (AMyGameStateBase* GS = World->GetGameState<AMyGameStateBase>())
		{
			for (ATopCharacter* Teammate : GS->FriendlyRoster)
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

	// ==============================================================================
	// 【服务器独裁第一阶段：强塞 Traveling 令牌，触发跨图黑幕】
	// 彻底废除旧版 RPC！全服统一下达强制旅行令，客户端本地自行拉起遮罩。
	// ==============================================================================
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* IterPC = It->Get())
		{
			// 【核心修改】：O(1) 极速提取成员变量
			if (AMyPlayerState* MyPS = IterPC->GetPlayerState<AMyPlayerState>())
			{
				if (UMyMapTravelStateComponent* NetPS = MyPS->MapTravelComponent)
				{
					NetPS->SetDeploymentStatus(ETravelDeploymentStatus::Traveling);
				}
			}
		}
	}

	// ==============================================================================
	// 【服务器独裁第二阶段：物理时序死等，绝不轮询客机网络】
	// 
	// 【逻辑升级补充】：为防客机网卡导致 ServerTravel 时黑幕尚未盖下而发生画面硬切，
	// 此处升级为 0.05s 状态栅栏：在物理时间走完的基础上，必须加入全员 UI 就绪状态的核验。
	// ==============================================================================
	double StartTime = World->GetTimeSeconds();

	// 强制物理等待时间：死等设计师配置的黑屏时间走完
	double RequiredPhysicalWait = OutConfig.ScreenOffDuration;

	// 💥【续命护航】：将极易被 GC 踩碎的局部 TravelURL 强行存进受 UPROPERTY 保护的锚点！
	PendingTravelURL = TravelURL;

	// 使用 TWeakObjectPtr 包装世界指针，防止等待期内世界被意外销毁导致野指针崩溃
	TWeakObjectPtr<UMyMapTravelSubsystem> WeakThis(this);
	TWeakObjectPtr<UWorld> WeakWorld(World);

	// 开启高频定时器，死等物理时间流逝完毕
	// 💥 闭包按值捕获列表不再捕获 TravelURL！彻底杀掉乱码隐患！
	World->GetTimerManager().SetTimer(SyncWaitTimerHandle, [WeakThis, WeakWorld, bIsAbsolute, bIsClientJoin, StartTime, RequiredPhysicalWait]()
		{
			// 唤醒时重新安全提取强引用，如果内存已经被销毁，这里会安全返回 nullptr
			UMyMapTravelSubsystem* StrongThis = WeakThis.Get();
			UWorld* SafeWorld = WeakWorld.Get();
			if (!StrongThis || !SafeWorld) return;

			double ElapsedTime = SafeWorld->GetTimeSeconds() - StartTime;

			// 💥 【跨图管线】：只保留纯净的物理倒计时！不在这里校验 Ack！
			// 等到新世界落地时，大管家自然会发 Ack 触发 WaitingForShell！
			if (ElapsedTime >= RequiredPhysicalWait)
			{
				// 满足条件，立刻销毁轮询器
				SafeWorld->GetTimerManager().ClearTimer(StrongThis->SyncWaitTimerHandle);

				// ==============================================================================
				// 🚀 【分流起航】：使用强引用的 PendingTravelURL，跨界敲门绝不乱码！
				// ==============================================================================
				if (bIsClientJoin)
				{
					StrongThis->TriggerClientTravelCommand(StrongThis->PendingTravelURL);
				}
				else
				{
					StrongThis->TriggerServerTravelCommand(StrongThis->PendingTravelURL, bIsAbsolute);
				}
			}
		}, 0.05f, true);
}


void UMyMapTravelSubsystem::TriggerClientTravelCommand(const FString& TravelURL)
{
	// ==============================================================================
	// 🚪 【副机管线】：局外客机拿着 IP / Session 地址强行敲开主机大门！
	// ==============================================================================
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			UE_LOG(LogTemp, Warning, TEXT("[MapTravelLog] 🚪 副机发起飞线连接。目标地址: %s"), *TravelURL);

			// 底层指令：强制客机本地断开当前世界，向指定服务器发起连接请求
			PC->ClientTravel(TravelURL, TRAVEL_Absolute);
		}
	}
}


void UMyMapTravelSubsystem::TriggerServerTravelCommand(const FString& TravelURL, bool bIsAbsolute)
{
	// ==============================================================================
	// 👑 【主机管线】：房主带队跨图/初次建房，全权主导物理位面的折叠！
	// ==============================================================================
	if (UWorld* World = GetWorld())
	{
		if (bIsAbsolute)
		{
			// 房主建房时，必须剥夺旧世界的无缝漫游防截断
			if (AGameModeBase* CurrentGM = World->GetAuthGameMode())
			{
				CurrentGM->bUseSeamlessTravel = false;
			}
		}

		UE_LOG(LogTemp, Warning, TEXT("[MapTravelLog] 👑 主机发起跨界/建房，引擎底层自动硬拽全服局内副机跟随！目标: %s"), *TravelURL);

		// 底层指令：主机强行切换当前世界，NetDriver 会自动将所有连接的局内副机硬拽过去！
		World->ServerTravel(TravelURL, bIsAbsolute);
	}
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
		// 💥【修改说明】架构迁移：唤醒队友时，同样需要去大屏幕 (GameState) 上提取队伍花名册。
		if (AMyGameStateBase* GS = World->GetGameState<AMyGameStateBase>())
		{
			for (ATopCharacter* Teammate : GS->FriendlyRoster)
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

	// ==============================================================================
	// 💥 【终极防线】：因为 InitGame 已经执行了开荒配置，这里一眼就能认出来！
	// 不查车票，不查字典，只要是开荒，直接退场，绝不碰 AI 队友！
	// ==============================================================================
	if (bIsCurrentMapInitialBoot)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MapTravelLog] 🏠 开荒状态保护生效：免除一切队伍强行排队，维持场景初始排放原状！"));

		// 【核心修复 1】：开荒时维持原状，但必须向系统宣告“物理地基已就绪”！
		// 否则玩家会被导演系统永远挂起在 WaitingForShell 状态！
		bIsPhysicalLayoutReady = true;

		// 【还原你的原版安全逻辑】：把由于地基未就绪而罚站的玩家安全拉起来！
		// 只有拿到 PlayerState 才执行，彻底消灭刚才日志里的“同步超时 (5秒)”！
		if (AMyGameModeBase* GM = Cast<AMyGameModeBase>(World->GetAuthGameMode()))
		{
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				if (APlayerController* IterPC = It->Get())
				{
					// 【核心修改】：改为 O(1) 直接访问组件成员变量
					if (AMyPlayerState* MyPS = IterPC->GetPlayerState<AMyPlayerState>())
					{
						if (UMyMapTravelStateComponent* PS = MyPS->MapTravelComponent)
						{
							if (PS->DeploymentStatus == ETravelDeploymentStatus::WaitingForShell && !IterPC->GetPawn())
							{
								GM->RestartPlayer(IterPC);
							}
						}
					}
				}
			}
		}

		// ==============================================================================
		// 💥【架构级修正】：开荒使命完成，优雅退场！
		// 随着上方的 RestartPlayer 执行完毕，场景内原生的假人已被夺舍，初次落地彻底大功告成！
		// 必须在此刻立刻关闭开荒锁！确保玩家后续的任何传送都能顺利进入正常的大一统排队管线！
		// ==============================================================================
		bIsCurrentMapInitialBoot = false;

		return;
	}

	// 声明目标接机点指针，准备进行全图搜索
	AMyUniversalDestination* TargetDest = nullptr;
	const FTransform* PrimaryPtr = nullptr;
	UDataLayerAsset* TargetDataLayer = nullptr;
	FTransform DestTransform = FTransform::Identity;

	// ==============================================================================
	// 【大一统闭环】：绝对的数据驱动寻址
	// ==============================================================================
	// 只有当大管家手中确实持有跨界车票 (PendingTravelRoute) 时，才去寻找传送门出口！
	if (GI->PendingTravelRoute)
	{
		// 💥 大一统：如果是同图，直接 O(1) 查高速字典拿 DataLayer 和坐标
		if (SameMapDestinationRegistry.Contains(GI->PendingTravelRoute))
		{
			PrimaryPtr = &SameMapDestinationRegistry[GI->PendingTravelRoute].TargetTransform;
			TargetDataLayer = SameMapDestinationRegistry[GI->PendingTravelRoute].BoundDataLayer;
		}
		else
		{
			// 跨图：此时新世界的 Persistent Level 已 Ready，目标必然在内存中，执行全局 Actor 扫盘 O(N)
			for (TActorIterator<AMyUniversalDestination> It(World); It; ++It)
			{
				if (AMyUniversalDestination* Dest = *It)
				{
					// 校验匹配：如果该接机点的监听列表中，包含大管家手里的这张车票
					if (Dest->ListeningRoutes.Contains(GI->PendingTravelRoute))
					{
						// 锁定目标接机点
						TargetDest = Dest;
						DestTransform = FTransform(Dest->GetActorRotation(), Dest->GetActorLocation() + FVector(0.0f, 0.0f, 15.0f), FVector::OneVector);
						PrimaryPtr = &DestTransform;
						TargetDataLayer = Dest->BoundDataLayer;
						// 寻址成功，立刻跳出高耗时的遍历循环
						break;
					}
				}
			}
		}
	}

	// 核心安全获取：提取绝对真实的本地玩家控制器，防死无缝漫游产生的幽灵假身
	APlayerController* PC = GetRealPlayerController(World);
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;

	// ==============================================================================
	// 🚀 大一统：执行统一的数据层流送！
	// ==============================================================================
	if (PendingSameMapDataLayer)
	{
		// 【同图专属管线】：预热早在 ExecuteSameMapTravel 就做过了，这里直接调用原有的斩杀接口！
		CommitSameMapDataLayer();
	}
	else if (TargetDataLayer)
	{
		// 【跨图专属管线】：跨图必须等新世界落地后才能提取 DataLayer，所以预热和刷新在此刻同时执行！
		UE_LOG(LogTemp, Warning, TEXT("[MapTravelLog] 🔄 执行跨图大一统数据层滑动窗口流送"));
		if (UMyDataLayerStreamingSubsystem* StreamingSub = World->GetSubsystem<UMyDataLayerStreamingSubsystem>())
		{
			StreamingSub->PreheatZoneBackground(TargetDataLayer);
			StreamingSub->RefreshSlidingWindow(TargetDataLayer, true);
		}
	}

	// 全局锁定落地状态（仅日志打印，实际物理计算已移交底层统一接口）
	// 💥 【核心修复】：修正假警报！同图有 PrimaryPtr，跨图有 TargetDest，只要有一个就算成功！
	if ((TargetDest || PrimaryPtr) && Pawn)
	{
		if (GI->PendingTravelRoute)
		{
			UE_LOG(LogTemp, Warning, TEXT("[MapTravelLog] 💥 瞬移成功！玩家真身强行穿插至目标路由: %s"), *GI->PendingTravelRoute->GetName());
		}
	}
	else if (Pawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MapTravelLog] ⚠️ 警告：未找到路由对应的接机点，采用默认出生点兜底。"));
	}

	// 🚀 逻辑拆分点 1：处理本地表现 (仅在 LocalPlayer 所在的机器执行)
	// (取代了已废弃的 HandleLocalPlayerSnapping)
	if (PC && PC->IsLocalController())
	{
		// 镜头视口对齐：强行把玩家控制器的相机视角拧过去，防止镜头滞后导致的平移拖影！
		if (TargetDest) PC->SetControlRotation(TargetDest->GetActorRotation());
		else if (PrimaryPtr) PC->SetControlRotation(PrimaryPtr->GetRotation().Rotator());
	}

	// 🚀 逻辑拆分点 2：处理权威部署 (单机开荒、Listen Server 主机执行)
	// 第一步：先释放小队记忆库中的 AI 队友躯壳进内存并注册名册
	if (World->GetNetMode() < NM_Client)
	{
		DeploySquadTeammates(World, TargetDest, GI);
	}

	// ==============================================================================
	// 【契约建立】：宣告物理地基就绪！捞起被 WaitingForShell 挂起的玩家控制器！
	// 第二步：执行 RestartPlayer，正式捏出主机/副机的真实肉体并注册进 FriendlyRoster 名册！
	// ==============================================================================
	bIsPhysicalLayoutReady = true;

	if (AMyGameModeBase* GM = Cast<AMyGameModeBase>(World->GetAuthGameMode()))
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (APlayerController* IterPC = It->Get())
			{
				// 【核心修改】：改为 O(1) 直接访问组件成员变量
				if (AMyPlayerState* MyPS = IterPC->GetPlayerState<AMyPlayerState>())
				{
					if (UMyMapTravelStateComponent* PS = MyPS->MapTravelComponent)
					{
						// 捞起之前因为没物理地基而在 WaitingForShell 罚站的控制器，重新走夺舍/捏人管线！
						if (PS->DeploymentStatus == ETravelDeploymentStatus::WaitingForShell && !IterPC->GetPawn())
						{
							GM->RestartPlayer(IterPC);
						}
					}
				}
			}
		}
	}

	// ==============================================================================
	// 💥【终极时序闭环：全员到齐，统一排队！】
	// 第三步：此时 AI 和 主机/副机的肉体全部生成完毕并注册进 FriendlyRoster 大名单！
	// 统一由服务器上帝视角统筹排队，主机必然稳稳抢占 C 位 (Offset=0)，全队瞬间对齐阵型！
	// ==============================================================================
	if (World->GetNetMode() < NM_Client)
	{
		// 重新拉取一次真实的本地 Pawn（因为刚在上面通过 RestartPlayer 捏出）
		APawn* NewlySpawnedPawn = PC ? PC->GetPawn() : nullptr;

		// 提取纯净的落地基准矩阵
		FTransform FinalBaseTransform = ResolveSafeDeploymentTransform(World, PrimaryPtr, NewlySpawnedPawn);

		// 全体肉体到位，正式执行上帝视角统一排兵布阵！
		ExecuteSquadFormationDeployment(World, FinalBaseTransform);
	}

	// ==============================================================================
	// 【联机时序加固】：已彻底剥夺房主在此处的“私自撕票权”！
	// 原先的 GI->PendingTravelRoute = nullptr; 已被删除。
	// 必须保留车票，直到 GameMode 的 StartPlay (NextTick) 确认所有首批客机都已安全连入，
	// 再由大管家统一销毁，彻底根绝客机网卡导致查票失败、掉回出生点的隐患。
	// ==============================================================================
}

void UMyMapTravelSubsystem::DeploySquadTeammates(UWorld* World, AMyUniversalDestination* Dest, UMyGameInstance* GI)
{
	// 💥 【核心拦截】：如果是同图漫游，大管家根本没装车，记忆库必定为空！
	// 直接退出！绝不执行下方无意义的空转与基准点探针警报！
	if (!GI || GI->SquadClassMemory.IsEmpty())
	{
		return;
	}

	APlayerController* PC = GetRealPlayerController(World);
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;

	// ==============================================================================
	// 【新框架大一统：跨图废弃 PlayerStart 挪位，全面拥抱上帝视角统筹】
	// ==============================================================================

	// 构建目标接机点的安全矩阵 (提取目标点的绝对坐标，Z轴强制增加 15cm 的高度冗余，作为完美防穿模的下落空间)
	FTransform DestTransform = FTransform::Identity;
	const FTransform* PrimaryPtr = nullptr;
	if (Dest)
	{
		DestTransform = FTransform(Dest->GetActorRotation(), Dest->GetActorLocation() + FVector(0.0f, 0.0f, 15.0f), FVector::OneVector);
		PrimaryPtr = &DestTransform;
	}

	// 💥 完美闭环：一行调用大一统接口，提取绝对纯净的坐标矩阵，取代原来 20 多行的冗余兜底！
	FTransform BaseTransform = ResolveSafeDeploymentTransform(World, PrimaryPtr, Pawn);

	// 第一步：只管把跨图记忆库里的 AI 躯壳释放出来
	for (TSubclassOf<ATopCharacter> TeammateClass : GI->SquadClassMemory)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		// 随便在基准点上方生出来即可，它的 BeginPlay 会自动将其注册进 GM->FriendlyRoster
		World->SpawnActor<ATopCharacter>(TeammateClass, BaseTransform.GetLocation() + FVector(0.0f, 0.0f, 500.0f), BaseTransform.GetRotation().Rotator(), SpawnParams);
	}

	// 🚀 内存优化：落地结束，立刻清空小队数组，释放类引用防止 TArray 内存泄漏！
	GI->SquadClassMemory.Empty();
	UE_LOG(LogTemp, Warning, TEXT("🧹 [内存优化] 小队记忆库已释放，等待下一轮装车。"));

	// 💥【核心时序修复】：此处坚决不发车！坚决不调用排队函数！
	// 必须等待 SnapPlayerToDestination 将挂起的主机/客机全部 RestartPlayer 捏出肉体后，再统一执行排兵布阵！
}

#pragma endregion


// ==============================================================================
// 玩家实体接管与落地部署导演系统 (Deployment Director System)
// ==============================================================================
#pragma region

void UMyMapTravelSubsystem::ExecuteInitialBootSetup()
{
	// 【专属开荒代码】：系统在地图刚加载完的瞬间，锁定开荒状态！
	UE_LOG(LogTemp, Warning, TEXT("[MapTravelLog] 🏠 执行专属开荒配置：锁定本局开荒状态，保护场景原生肉体不被排队拉扯！"));
	bIsCurrentMapInitialBoot = true;
}

bool UMyMapTravelSubsystem::ExecuteDeploymentDirector(AController* NewPlayer, AMyGameModeBase* GameMode, float CurrentWaitTime)
{
	if (!NewPlayer || !GetWorld() || !GameMode) return false;

	// ==============================================================================
	// 💥 增加“同步超时”强制放行 (Deadlock Protection)
	// ==============================================================================
	UMyMapTravelStateComponent* PS = nullptr;

	// O(1) 提取：强转出原生宿主，直接拿到焊死的成员变量
	if (AMyPlayerState* MyPS = NewPlayer->GetPlayerState<AMyPlayerState>())
	{
		PS = MyPS->MapTravelComponent;
	}

	if (!PS)
	{
		if (CurrentWaitTime >= 5.0f)
		{
			UE_LOG(LogTemp, Error, TEXT("🚨 [DeploymentDirector] 同步超时 (5秒)！"));

			// 生化6双主角铁律：如果是客机，没同步上直接判死刑，绝不生成第三个人！
			if (!NewPlayer->IsLocalController()) return true;

			// 主机兜底：强制放行，执行动态捏人
			return ExecuteDynamicSquadArrival(NewPlayer, GameMode);
		}

		// 挂起 0.1 秒后递归轮询，等待客机 PlayerState 网络状态就绪
		FTimerHandle RetryTimer;
		TWeakObjectPtr<UMyMapTravelSubsystem> WeakThis(this);
		TWeakObjectPtr<AController> WeakPlayer(NewPlayer);
		TWeakObjectPtr<AMyGameModeBase> WeakGM(GameMode);

		GetWorld()->GetTimerManager().SetTimer(RetryTimer, [WeakThis, WeakPlayer, WeakGM, CurrentWaitTime]()
			{
				if (UMyMapTravelSubsystem* StrongThis = WeakThis.Get())
				{
					if (AController* PC = WeakPlayer.Get())
					{
						if (AMyGameModeBase* GM = WeakGM.Get())
						{
							StrongThis->ExecuteDeploymentDirector(PC, GM, CurrentWaitTime + 0.1f);
						}
					}
				}
			}, 0.1f, false);

		return true; // 拦截原生生成，等待同步
	}

	// ==============================================================================
	// 💥 【令牌契约 1：强制挂起】
	// 如果服务器底层的物理阵型（2.5D 排队或射线落地）还没搞定，任何连入的副机必须强制挂起！
	// ==============================================================================
	if (!bIsPhysicalLayoutReady)
	{
		// 💥【修复】：永远先打印拦截日志，再下发令牌！防止同步状态机导致的时序倒错
		UE_LOG(LogTemp, Warning, TEXT("🛑 [DeploymentDirector] 物理阵型未就绪，拦截生成，强制玩家进入 WaitingForShell 挂起状态！"));
		PS->SetDeploymentStatus(ETravelDeploymentStatus::WaitingForShell);
		return true;
	}

	bool bDeploymentSuccess = false;

	// ==============================================================================
	// 💥 【终极联机修复：主副机管线物理级劈开】
	// 副机（Client）毫无决策权！它只需要躺好，乖乖接管服务器大名单里留好的待命躯壳！
	// ==============================================================================
	if (!NewPlayer->IsLocalController())
	{
		// ------------------------------------------------------------------------------
		// 【管线 A：客机中途加入 / 跨图落地跟随】
		// 架构核心 (基于 Epic Developer Assistant 方案二深度解析)：
		// 1. 不可逃避的自动复制：服务器 Spawn 肉体后，NetDriver 会自动将其打包发给客机。
		//    若客机本地手动生成肉体，会导致“幽灵 Actor”与“镜像副本”互相冲突卡死。
		// 2. 物理主权悖论：客机本地生成的 Actor 没有网络主权 (NetGUID)，无法被夺舍。
		// 3. 消灭 105 帧延迟：服务器先行对齐阵型，镜像副本在客机本地生成时就已物理静止。
		//    客机不需要发信号，自动夺舍后 UI 亮屏，睁眼即是完美阵型，彻底消灭坐标跳变！
		// ------------------------------------------------------------------------------
		UE_LOG(LogTemp, Warning, TEXT("[DeploymentDirector] 判定为：【副机/客机连入】。下发管线 -> 剥夺决策权，强制接管大名单待命躯壳！"));
		bDeploymentSuccess = ExecuteGuestDeployment(NewPlayer, GameMode);

		if (!bDeploymentSuccess)
		{
			// 连躯壳都找不到，直接宣判死刑，打入 Failed 状态
			PS->SetDeploymentStatus(ETravelDeploymentStatus::Failed);
			UE_LOG(LogTemp, Error, TEXT("🚨 [DeploymentDirector] 致命拦截：场景内无多余主角躯壳！拒绝客机生成新肉体，加入已被拒绝。"));
			return true; // 成功拦截 GameMode 原生生成，防止副机被凭空捏出来
		}
	}
	else
	{
		// ==============================================================================
		// 💥 以下为主机（Server/Host）绝对决策领域！
		// 主机负责查表、判相性、找预设件或亲自下达 SpawnActor 指令！
		// ------------------------------------------------------------------------------
		// 【管线 B：主机开荒 /带着副机跨图】
		// 主机充当造物主，执行 Epic Developer Assistant 所说的“先行预制”机制，
		// 统筹 Spawn 所有人的肉体并进行中心化排队，为管线 A 提供稳定副本。
		// ==============================================================================
		UMyGameInstance* GI = GetWorld()->GetGameInstance<UMyGameInstance>();
		if (GI)
		{
			// 强行剥离 PIE 前缀，获取绝对纯净的 MapName 供查表
			FString CleanMapName = UGameplayStatics::GetCurrentLevelName(GetWorld(), true);
			EMapPhaseType MapType = GI->GetMapPhase(CleanMapName);

			// 逻辑分水岭 1：这张图是否有开荒设定？
			if (MapType == EMapPhaseType::HasPioneeringPhase)
			{
				// 逻辑分水岭 2：这张图的开荒期是否已经结束？
				if (bIsCurrentMapInitialBoot)
				{
					UE_LOG(LogTemp, Warning, TEXT("[DeploymentDirector] 判定为：【首次开荒图】。下发管线 -> 主机夺舍原生肉体！"));
					// 【这是开荒！】执行夺舍原生假人，此时不排队。
					bDeploymentSuccess = ExecutePioneeringPossession(NewPlayer, GameMode);
				}
			}

			if (!bDeploymentSuccess)
			{
				// 【走到这里说明：或者是 AlwaysDynamic 地图，或者是已过开荒期的回访图】
				UE_LOG(LogTemp, Warning, TEXT("[DeploymentDirector] 判定为：【动态生成图/回访图】。下发管线 -> 传送门动态捏人排队！"));
				bDeploymentSuccess = ExecuteDynamicSquadArrival(NewPlayer, GameMode);
			}
		}
	}

	// ==============================================================================
	// 💥 【令牌契约 2：发牌解封】
	// 只要肉体分配（无论是夺舍还是生成）成功，服务器立刻翻转令牌，通知客机解封！
	// ==============================================================================
	if (bDeploymentSuccess)
	{
		PS->SetDeploymentStatus(ETravelDeploymentStatus::ReadyToPossess);
	}

	return bDeploymentSuccess;
}

bool UMyMapTravelSubsystem::ExecuteGuestDeployment(AController* NewPlayer, AMyGameModeBase* GameMode)
{
	// 【副机专属管线】：剥夺一切 SpawnActor 的权力，只准寻找大名单中的待命队友！
	ATopCharacter* TargetShell = FindSquadTeammateShell(GetWorld(), GameMode);

	if (TargetShell)
	{
		NewPlayer->Possess(TargetShell);
		// 跨系统回调：通过 GameMode 的公开接口完成网络收尾与 2.5D 镜头对齐
		GameMode->ExecuteFinishRestartPlayer(NewPlayer, TargetShell->GetActorRotation());
		UE_LOG(LogTemp, Warning, TEXT("🎯 [副机部署管线] 客机成功接管服务器分配的待命队友！"));
		return true;
	}

	return false;
}

bool UMyMapTravelSubsystem::ExecutePioneeringPossession(AController* NewPlayer, AMyGameModeBase* GameMode)
{
	// 【主机专属管线 A】：只准寻找场景中原生摆放的假人
	ATopCharacter* TargetShell = FindInitialStartupShell(GetWorld());

	if (TargetShell)
	{
		NewPlayer->Possess(TargetShell);
		GameMode->ExecuteFinishRestartPlayer(NewPlayer, TargetShell->GetActorRotation());
		UE_LOG(LogTemp, Warning, TEXT("🎯 [主机开荒管线] 房主成功夺舍场景原生初始躯壳！"));
		return true;
	}

	return false;
}

bool UMyMapTravelSubsystem::ExecuteDynamicSquadArrival(AController* NewPlayer, AMyGameModeBase* GameMode)
{
	// 兜底：使用 GameMode 蓝图里配置的默认角色
	UClass* ClassToSpawn = GameMode->DefaultPawnClass;
	UMyGameInstance* GI = GetWorld()->GetGameInstance<UMyGameInstance>();

	if (GI && NewPlayer->PlayerState)
	{
		// 防御性唯一标识符 (防局域网碰撞)，提取网络底层唯一 ID 或本地 ID
		FString NetId = NewPlayer->PlayerState->GetUniqueId().IsValid() ? NewPlayer->PlayerState->GetUniqueId().ToString() : FString::Printf(TEXT("LOCAL_PLAYER_%d"), NewPlayer->PlayerState->GetPlayerId());

		// 从大管家记忆库里提取该玩家跨图前的专属蓝图类
		if (TSubclassOf<ATopCharacter>* SavedClass = GI->PlayerClassMemory.Find(NetId))
		{
			if (*SavedClass) ClassToSpawn = *SavedClass;
		}
	}

	// 图纸都没有，放弃生成
	if (!ClassToSpawn)
	{
		// 💥 【防御性基建】：报错预警！
		UE_LOG(LogTemp, Error, TEXT("🚨 [DeploymentDirector] 动态捏人失败：ClassToSpawn 为空！请检查 GameMode 中的 DefaultPawnClass 是否被置空，或者服务器跨图时未同步该角色的图纸！"));
		return false;
	}

	// ==============================================================================
	// 💥 重构剥离：纯粹的躯壳生成 (Pure Shell Spawning)
	// 彻底放弃在此处寻找传送门与计算落地！全权移交给后期的上帝排队管线统一搬运！
	// ==============================================================================

	// 直接寻找地图原配的 PlayerStart 作为临时产房
	AActor* StartSpot = GameMode->FindPlayerStart(NewPlayer);
	FTransform SpawnTransform = StartSpot ? FTransform(StartSpot->GetActorRotation(), StartSpot->GetActorLocation(), FVector::OneVector) : FTransform::Identity;

	// 强制物理降临！
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	SpawnParams.Owner = NewPlayer;

	// 亲自调用 SpawnActor，在临时产房捏出肉体
	if (APawn* NewBody = GetWorld()->SpawnActor<APawn>(ClassToSpawn, SpawnTransform, SpawnParams))
	{
		NewPlayer->Possess(NewBody);

		// 闭环底层网络管线，同步正确的初始朝向给服务器
		GameMode->ExecuteFinishRestartPlayer(NewPlayer, SpawnTransform.GetRotation().Rotator());

		// ==============================================================================
		// 💥 核心修正：绝对静止！
		// 剥夺原本的 MOVE_Walking 软着陆！在黑幕期间，新肉体必须处于绝对冻结状态！
		// 它的最终精确坐标、2.5D朝向和射线贴地，将由 SnapPlayerToDestination 统一接管和传送！
		// ==============================================================================
		if (ACharacter* NewChar = Cast<ACharacter>(NewBody))
		{
			if (UCharacterMovementComponent* CMC = NewChar->GetCharacterMovement())
			{
				CMC->DisableMovement();
			}
		}

		UE_LOG(LogTemp, Warning, TEXT("🚀 [动态捏人管线] 成功生成临时静止肉体，等待上帝视角统一搬运！"));
		return true;
	}

	return false;
}

ATopCharacter* UMyMapTravelSubsystem::FindInitialStartupShell(UWorld* World)
{
	// 声明并初始化开荒空壳指针，用于接收场景原生角色
	ATopCharacter* FoundShell = nullptr;

	for (TActorIterator<ATopCharacter> It(World); It; ++It)
	{
		ATopCharacter* Character = *It;

		// 跳过空指针或正在播放销毁动画的残骸
		if (!Character || Character->IsActorBeingDestroyed()) continue;

		// 筛选出当前没有控制器附身的无主躯壳
		if (Character->GetController() == nullptr)
		{
			// 1. RF_WasLoaded: 只要是从地图文件里读出来的，必定有此标记。
			// 2. !RF_Transient: 绝对排除运行时的残影垃圾。
			if (Character->HasAnyFlags(RF_WasLoaded) && !Character->HasAnyFlags(RF_Transient))
			{
				if (FoundShell == nullptr)
				{
					// 第一顺位原生躯壳：锁定为夺舍目标
					FoundShell = Character;
				}
				else
				{
					UE_LOG(LogTemp, Display, TEXT("🛡️ [GameMode] 保留额外的合法预设件: %s"), *Character->GetName());
				}
			}
		}
	}

	return FoundShell;
}

ATopCharacter* UMyMapTravelSubsystem::FindSquadTeammateShell(UWorld* World, AMyGameModeBase* GameMode)
{
	// 声明并初始化空壳指针，用于接收小队队友
	ATopCharacter* FoundShell = nullptr;
	if (!GameMode) return nullptr;

	// 💥【修改说明】提前从世界上下文中提取 GameState，
	// 联机状态下副机连入时必须依靠大屏幕上的数据来核对躯壳是否合法。
	AMyGameStateBase* GS = World->GetGameState<AMyGameStateBase>();
	if (!GS) return nullptr;

	for (TActorIterator<ATopCharacter> It(World); It; ++It)
	{
		ATopCharacter* Character = *It;

		if (!Character || Character->IsActorBeingDestroyed()) continue;

		// 筛选出当前没有控制器附身的无主躯壳
		if (Character->GetController() == nullptr)
		{
			// 1. 副机连入时：房主动态生成的待命队友没有 RF_WasLoaded，但已注册进 FriendlyRoster 大名单！
			// 2. !RF_Transient: 确保它不是运行时产生的临时垃圾或预览对象
			// 💥【修改说明】使用 GS->FriendlyRoster 替代废弃的 GameMode->FriendlyRoster 进行合法性比对。
			if (!Character->HasAnyFlags(RF_Transient) && GS->FriendlyRoster.Contains(Character))
			{
				if (FoundShell == nullptr)
				{
					// 找到合法的动态队友，选定为夺舍目标
					FoundShell = Character;
				}
				else
				{
					// 【核心逻辑保护】：保留房主同步过来的其余待命队友，绝不盲目清场！
					UE_LOG(LogTemp, Display, TEXT("🛡️ [GameMode] 保留额外的待命队友: %s"), *Character->GetName());
				}
			}
		}
	}
	return FoundShell;
}

FTransform UMyMapTravelSubsystem::ResolveSafeDeploymentTransform(UWorld* World, const FTransform* PrimaryTransform, AActor* FallbackEntity)
{
	FVector SafeLocation = FVector::ZeroVector;
	FRotator SafeRotation = FRotator::ZeroRotator;
	bool bFound = false;

	// ==============================================================================
	// 💥 【大一统兜底管线：物理极限抢救】
	// ==============================================================================

	if (PrimaryTransform)
	{
		// 正常管线：接机点健在，完美提取物理坐标
		SafeLocation = PrimaryTransform->GetLocation();
		SafeRotation = PrimaryTransform->GetRotation().Rotator();
		bFound = true;
	}
	else if (FallbackEntity)
	{
		// 🎯 兜底：如果没有找到接机点（场景里没放，或者策划漏配了）
		// 强行提取玩家踩传送门时的原位坐标，或现有的兜底位置
		// 宁可让玩家原地黑屏闪一下，重新落回原地，也绝不让管线崩溃或掉入虚空！
		SafeLocation = FallbackEntity->GetActorLocation();
		SafeRotation = FallbackEntity->GetActorRotation();
		bFound = true;
		UE_LOG(LogTemp, Warning, TEXT("[MapTravelLog] ⚠️ 警告：未找到路由对应的接机点，采用实体当前位置兜底。"));
	}
	else if (World)
	{
		// 【无空壳且非跨图，或跨图失败兜底】 -> 兜底找初始出生点 (PlayerStart)
		// 终极兜底：主机都没生出来，或连触发实体都没有时，找地图原配起点
		UE_LOG(LogTemp, Warning, TEXT("[MapTravelLog] ⚠️ 警告：无实体可用！启动终极兜底：寻找 PlayerStart"));
		if (AActor* PS = UGameplayStatics::GetActorOfClass(World, APlayerStart::StaticClass()))
		{
			SafeLocation = PS->GetActorLocation();
			SafeRotation = PS->GetActorRotation();
			bFound = true;
		}
	}

	if (bFound)
	{
		// 同样，绝对不能锁 Y 轴！直接取 3D 空间里的真实坐标。
		// 仅强制锁定角色的 Scale 为 1:1:1，防止受到传送门缩放的污染。
		return FTransform(SafeRotation, SafeLocation, FVector::OneVector);
	}

	return FTransform::Identity;
}

void UMyMapTravelSubsystem::ExecuteSquadFormationDeployment(UWorld* World, const FTransform& BaseTransform)
{
	if (!World) return;

	// 💥【修改说明】排队系统现在必须由 GameState 提供权威的实体名单。
	// 原 GameMode 中的名单已作废。
	AMyGameStateBase* GS = World->GetGameState<AMyGameStateBase>();
	if (!GS) return;

	// 提取安全兜底坐标的绝对地板 Z 坐标，杜绝 90cm 悬空
	float BaseFloorZ = BaseTransform.GetLocation().Z;

	// 💥 修复【坐标偏离Bug】：不再强加给绝对X轴，而是提取基准朝向的绝对“右方向向量”！
	FVector RightVector = BaseTransform.GetRotation().GetAxisY();

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

	// 💥 修复【抢C位碰撞Bug】：不再从0开始排！AI和副机强制从 120 偏移开始！绝不占用 0 的位置！
	float CurrentXOffset = 120.0f;
	bool bFlipOffset = true;

	// 直接遍历已经存在于当前地图上的队友肉体
	// 💥【修改说明】遍历大屏幕上的名单进行阵型展开
	for (ATopCharacter* Teammate : GS->FriendlyRoster)
	{
		// 1. 基础安全校验
		if (!Teammate || Teammate->IsActorBeingDestroyed()) continue;

		float TargetOffset = 0.0f;

		// 💥 核心甄别：只有受本地玩家控制的真身，才有资格霸占正中心(0点)！其他人全部靠边！
		if (Teammate->GetController() && Teammate->GetController()->IsLocalController())
		{
			TargetOffset = 0.0f;
		}
		else
		{
			// 其他人乖乖按 120, -120, 240, -240 排队
			TargetOffset = bFlipOffset ? CurrentXOffset : -CurrentXOffset;

			// 每排完一左一右，间距拉大
			if (!bFlipOffset)
			{
				CurrentXOffset += 120.0f;
			}
			bFlipOffset = !bFlipOffset;
		}

		// 完美沿目标横向展开 (沿着传送门的左右两边排队，再也不会重叠成一排)
		FVector OffsetLoc = BaseTransform.GetLocation() + RightVector * TargetOffset;

		// 贴地短探针，寻找真实地板
		FHitResult GroundHit;
		// 从预估地板的上方 50cm 往下探 100cm，避免从高空打中隐形门框
		FVector TraceStart = OffsetLoc + FVector(0.0f, 0.0f, 150.0f);
		FVector TraceEnd = OffsetLoc - FVector(0.0f, 0.0f, 300.0f);

		FCollisionQueryParams Params;
		Params.AddIgnoredActors(IgnoredActorsForTrace);

		// 统一物理标准：使用 ECC_WorldStatic 绝对地形探针
		if (World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_WorldStatic, Params))
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

		// 核心暗箱：强行将所有名册里的躯壳折叠至计算好的完美阵型！
		// 底层 Network Replicated Movement 会瞬间剥夺客机的物理反抗权，杜绝拉扯弹射！
		Teammate->TeleportTo(OffsetLoc, BaseTransform.GetRotation().Rotator(), false, true);

		// 物理点穴：在黑屏彻底解开前，锁死全队重力坠落，保证完美滞空等UI开幕
		if (UCharacterMovementComponent* CMC = Teammate->GetCharacterMovement())
		{
			CMC->DisableMovement();
		}

		// 将刚落地成功的队友立刻加入黑名单，绝不干扰下一个人的探测
		IgnoredActorsForTrace.Add(Teammate);
		UE_LOG(LogTemp, Warning, TEXT("🚀 [上帝视角排阵] 肉体已成功分配阵型并完成物理搬运: %s"), *Teammate->GetName());
	}
}

#pragma endregion
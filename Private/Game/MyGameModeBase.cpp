// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/MyGameModeBase.h"
// 角色
#include "Character/TopCharacter.h"

// 引入后台预热所需的底层系统头文件
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"
#include "SaveGame/Subsystem/MySaveSubsystem.h"
#include "EngineUtils.h"

// 【新增】：为了让 AI 重新接管躯壳，必须引入你的 AI 控制器头文件
#include "AI/Controller/MyAIController.h"

#include "Game/MyGameInstance.h"
#include "GameFramework/PlayerState.h"
#include "Streaming/MyDataLayerStreamingSubsystem.h"


// ==============================================================================
// 生命周期 (Lifecycle)
// ==============================================================================
#pragma region

AMyGameModeBase::AMyGameModeBase()
{
	// 【核心配置】：在构造函数中开启无缝旅行支持，允许跨地图动态无缝加载，消除黑屏与连接中断
	// 这样引擎在加载此类时 (CDO阶段)，就会将其刻入默认配置
	bUseSeamlessTravel = true;
}

void AMyGameModeBase::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	// 【架构解耦与时序修复】：将数据层流送从 BeginPlay 提前到世界刚建立、玩家尚未连入的阶段！
	// 【架构解耦】：GameMode 绝不直接干涉底层数据层状态，全权移交给专职的流送子系统去裁判！
	if (StarterDataLayer)
	{
		// 跨系统寻址：尝试获取专职负责大世界流送调度的子系统，实现严格的职责分离
		if (UMyDataLayerStreamingSubsystem* StreamingSub = GetWorld()->GetSubsystem<UMyDataLayerStreamingSubsystem>())
		{
			// 将初始数据层的最终生杀大权移交，由流送子系统根据“开荒”或“回访”状态进行 O(0) 的物理流送拦截
			StreamingSub->ResolveStarterDataLayer(StarterDataLayer);

			// 🚀 优化实装：防御性提取世界上下文，防止极端情况下的空指针崩溃
			if (UWorld* World = GetWorld())
			{
				UE_LOG(LogTemp, Warning, TEXT("⏳ [GameMode] 正在强制阻塞主线程，等待初始数据层 (StarterDataLayer) 载入内存..."));

				// 【终极强同步】：强制阻塞引擎，让 CPU 停下来等硬盘！
				// 直到初始数据层完全加载完毕，才允许引擎继续往下执行，放行玩家连接。
				// 这保证了接下来执行 RestartPlayer 寻址时，场景假人 100% 已经加载进内存！
				// 确保 StarterDataLayer 保持轻量化，如果阻塞时间超过了网络层的 PendingNetGame 超时时间，客机可能会因为服务器无响应而连接失败
				// 
				// 【底层原理深度剖析】：BlockTillLevelStreamingCompleted() 是一个“全局同步点”。
				// 1. 真空期：在 InitGame 执行的瞬间，整个 UWorld 除了刚申请的 StarterDataLayer，没有任何其他流送任务。
				// 2. 清空队列：该函数会暴力清空当前世界的所有流送待办请求 (Pending Requests)，强迫主线程处理硬盘 IO 和 UObject 序列化。
				// 3. 顺便的必然：因为它负责清空队列，而我们刚刚精准塞入了唯一的任务，所以它必定会完美等到我们的初始数据层全部“物理存在”于内存中，才会放行。
				World->BlockTillLevelStreamingCompleted();

				UE_LOG(LogTemp, Warning, TEXT("✅ [GameMode] 初始数据层 Actor 已全部就绪，放行玩家连接管线！"));
			}
		}
	}
}

void AMyGameModeBase::StartPlay()
{
	// GameMode 的 StartPlay 是所有 Actor 准备就绪、游戏正式开始的冲锋号
	Super::StartPlay();

	// 【核心时序控制：第一阶段 - 读档分发微操与联机车票清算 (NextTick Deferral)】

	// 尝试获取当前游戏世界的全局实例 (GameInstance)，它是跨关卡存活的最高级别实体
	if (UGameInstance* GI = GetGameInstance())
	{
		// 从游戏实例中，精准拉取负责统筹所有存档逻辑的大管家 (MySaveSubsystem)
		if (UMySaveSubsystem* SaveSub = GI->GetSubsystem<UMySaveSubsystem>())
		{
			// 🚀 优化实装：使用 TWeakObjectPtr 防范 Lambda 异步执行时的空指针击穿崩溃
			TWeakObjectPtr<UMySaveSubsystem> WeakSaveSub(SaveSub);

			// 【联机时序加固】：额外捕获大管家指针，准备在全员就绪后安全撕票
			TWeakObjectPtr<UMyGameInstance> WeakGI(Cast<UMyGameInstance>(GI));

			GetWorld()->GetTimerManager().SetTimerForNextTick([WeakSaveSub, WeakGI]()
				{
					if (WeakSaveSub.IsValid())
					{
						// 此时世界绝对安全，大管家正式下场，给各个已就绪的 Actor 分发读档数据
						WeakSaveSub->HandlePendingLoad();
					}

					// ==============================================================================
					// 【终极联机时序修复：全员到齐后的统一撕票】
					// 坚决剥夺房主在 SnapPlayerToDestination 里的私自撕票权！
					// 确保客机无论网络多慢，在执行 RestartPlayer 时大管家手里都还有车票！
					// 直到 NextTick (所有首批主客机实体都已 Spawn 完毕)，再统一销毁，防止中途飞线的玩家误读车票。
					// ==============================================================================
					if (WeakGI.IsValid() && WeakGI->PendingTravelRoute != nullptr)
					{
						WeakGI->PendingTravelRoute = nullptr;
						UE_LOG(LogTemp, Warning, TEXT("🎫 [跨图管线] 首批玩家部署与读档完毕，大管家正式销毁跨界车票！"));
					}
				});
		}
	}


	// 【核心时序控制：第二阶段 - 后台基建任务预热 (Background Infrastructure Warm-up)】

	// 规范解法：遵循 UE 5.8 内存安全规范，使用 TWeakObjectPtr 弱引用捕获 GameMode 自身 (this)
	TWeakObjectPtr<AMyGameModeBase> WeakThis(this);

	// 声明一个定时器句柄，用于在底层追踪并管理这个 2 秒的延时任务
	FTimerHandle SaveWarmupTimer;

	// 架构目的：错峰出行（CPU Peak Shaving），避开地图刚加载时渲染层构建和 Shader 编译的性能算力尖峰。
	// 设置一个 2 秒的后台定时器，将高耗时的 IO 预载作业平滑推迟
	GetWorld()->GetTimerManager().SetTimer(SaveWarmupTimer, [WeakThis]()
		{
			// 极其优雅的内存安全判定：静默查验 2 秒后的 GameMode (this) 是否依然存活，若世界毁灭则静默失效，绝不越界
			if (WeakThis.IsValid())
			{
				if (UGameInstance* GI = WeakThis->GetGameInstance())
				{
					if (UMySaveSubsystem* SaveSub = GI->GetSubsystem<UMySaveSubsystem>())
					{
						// 此时地图加载的 CPU 峰值已平息，利用后台低负载期平滑执行高耗时的硬盘拉取作业
						SaveSub->PreloadRegistry();
					}
				}
			}
		}, 2.0f, false); // 2.0f 代表延时 2 秒执行，false 代表不循环（仅触发一次）
}

void AMyGameModeBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 关卡卸载或点击编辑器停止按钮时触发：彻底消除所有挂起的 Timer
	if (UWorld* World = GetWorld())
	{
		// 1. 清除 GameMode 自身挂起的计时器（比如 2 秒的后台基建预热任务）
		World->GetTimerManager().ClearAllTimersForObject(this);
	}

	Super::EndPlay(EndPlayReason);
}

void AMyGameModeBase::BeginPlay()
{
	// 这里的流送代码已经被移走到 InitGame()

	Super::BeginPlay();
}

#pragma endregion


// ==============================================================================
// 队伍名册系统 (Squad Roster System)
// ==============================================================================
#pragma region

void AMyGameModeBase::RegisterFriendly(ATopCharacter* Character)
{
	if (Character)
	{
		// 在数组里从头到尾扫一遍，看看有没有一模一样的指针，没有则塞入
		FriendlyRoster.AddUnique(Character);

		// 向全宇宙广播：友军名单已更新！(不在乎谁在听，完全解耦)
		OnRosterChanged.Broadcast();
	}
}

void AMyGameModeBase::UnregisterFriendly(ATopCharacter* Character)
{
	if (Character)
	{
		// 遵循你的设计：目前直接移除，未来可扩展为灰色显示
		// 放弃冷冰冰的系统内置排序，可以让玩家自主手动调整位置
		FriendlyRoster.Remove(Character);

		// 向全宇宙广播：友军名单已更新！
		OnRosterChanged.Broadcast();
	}
}

#pragma endregion


// ==============================================================================
// 无缝旅行与状态流转 (Seamless Travel)
// ==============================================================================
#pragma region

void AMyGameModeBase::GetSeamlessTravelActorList(bool bToTransition, TArray<AActor*>& ActorList)
{
	Super::GetSeamlessTravelActorList(bToTransition, ActorList);

	// 目前保持完全空白，不强制干涉任何飞行物或 Actor 的跨地图流转。
	// 一切由引擎默认规则处理，新关卡将是一个干干净净的新开局。
}

#pragma endregion


// ==============================================================================
// 联机底层探针与接客管线 (Network Probes & Player Spawning)
// ==============================================================================
#pragma region

void AMyGameModeBase::PostLogin(APlayerController* NewPlayer)
{
	UE_LOG(LogTemp, Warning, TEXT("========================================================="));
	UE_LOG(LogTemp, Warning, TEXT("🟢 [GameMode] 客机玩家正式完成连接！Controller: %s"), *GetNameSafe(NewPlayer));

	Super::PostLogin(NewPlayer);
}

void AMyGameModeBase::RestartPlayer(AController* NewPlayer)
{
	// 基础安全拦截：确保玩家控制器与世界上下文有效
	if (!NewPlayer || !GetWorld()) return;

	// 【防线 1：绝对放行】如果玩家手里已有肉体（无缝漫游携带或 AutoPossess），直接放行
	if (NewPlayer->GetPawn()) return;

	// 声明接收场景原生角色或小队队友的空壳指针
	ATopCharacter* TargetShell = nullptr;

	// 🚀 逻辑拆分点：判定当前连入的是否为本地房主（Listen Server 的 Host）
	// 注意：在 Listen Server 中，房主的 PlayerController.IsLocalController() 为 true，客机的为 false。
	const bool bIsLocalHost = NewPlayer->IsLocalController();

	if (bIsLocalHost)
	{
		// 【主机开荒管线】：只准寻找场景中通过 DataLayer 加载的“原生预设件” (RF_WasLoaded)
		// 因为主机连入时，世界上还没有任何动态生成的队友！
		UE_LOG(LogTemp, Warning, TEXT("🏠 [Host-Pipe] 本地房主正在接管地图原生躯壳..."));
		TargetShell = FindInitialStartupShell();
	}
	else
	{
		// 【客机接管管线】：优先寻找房主已经同步过来的“无主小队队友” (FriendlyRoster)
		// 因为此时房主早已落地，并已经 Spawn 出了队友肉体同步给了客机。
		UE_LOG(LogTemp, Warning, TEXT("✈️ [Guest-Pipe] 客机玩家正在接管已同步的队友躯壳..."));
		TargetShell = FindSquadTeammateShell(NewPlayer);
	}

	// ==============================================================================
	// 【分支 A：开荒期与小队跨图期的寻址与夺舍】
	// ==============================================================================
	// 找到了合法的无主角色，直接附身接管，完美继承它在场景里原有的 2.5D 朝向！
	if (TargetShell)
	{
		// 夺舍执行：将当前登录玩家的灵魂 (PlayerController) 强行注入该预设躯壳或小队队友
		NewPlayer->Possess(TargetShell);
		// 闭环管线：完成玩家重启的网络收尾工作，并提取躯壳在场景中的真实旋转值（2.5D朝向）同步给服务器
		FinishRestartPlayer(NewPlayer, TargetShell->GetActorRotation());
		UE_LOG(LogTemp, Warning, TEXT("🎯 [数据层开荒管线] 玩家成功接管场景预设的初始躯壳或待命队友！"));
		return;
	}

	// ==============================================================================
	// 【分支 B：回访期动态捏人】
	// ==============================================================================
	UClass* ClassToSpawn = DefaultPawnClass; // 兜底：使用 GameMode 蓝图里配置的默认角色

	UMyGameInstance* GI = GetWorld()->GetGameInstance<UMyGameInstance>();

	// 【修复点 1】：只要大管家手里拿着车票，就绝对是跨图状态！绝不能依赖网络 ID 查表来判定！
	bool bIsTraveling = (GI && GI->PendingTravelRoute != nullptr);

	if (GI)
	{
		if (APlayerState* PS = NewPlayer->PlayerState)
		{
			// 防御性唯一标识符 (防局域网碰撞)，提取网络底层唯一 ID 或本地 ID
			FString NetId = PS->GetUniqueId().IsValid() ? PS->GetUniqueId().ToString() : FString::Printf(TEXT("LOCAL_PLAYER_%d"), PS->GetPlayerId());

			// 从大管家记忆库里提取该玩家跨图前的专属蓝图类
			if (TSubclassOf<ATopCharacter>* SavedClass = GI->PlayerClassMemory.Find(NetId))
			{
				if (*SavedClass)
				{
					ClassToSpawn = *SavedClass;
				}
			}
		}
	}

	if (!ClassToSpawn) return; // 图纸都没有，放弃生成

	// 寻找大一统接机点（跨图使用）或出生点（未配置接机点时的兜底）
	FTransform SpawnTransform = FTransform::Identity;

	if (bIsTraveling && GI)
	{
		// 【跨图传送】 -> 去找大一统传送门出口 (AMyUniversalDestination)
		if (GI->PendingTravelRoute)
		{
			for (TActorIterator<AMyUniversalDestination> It(GetWorld()); It; ++It)
			{
				if (AMyUniversalDestination* Dest = *It)
				{
					// 必须查票：只有对应了车票的接机点，才是真正的目的地
					if (Dest->ListeningRoutes.Contains(GI->PendingTravelRoute))
					{
						// ==================== 【撤回错误优化，恢复 3D 纵深】 ====================
						// 绝对不能强锁 Y 轴！直接取传送门在 3D 空间里的真实坐标。
						// 仅强制锁定角色的 Scale 为 1:1:1，防止受到传送门缩放的污染。
						SpawnTransform = FTransform(Dest->GetActorRotation(), Dest->GetActorLocation(), FVector::OneVector);
						// ==============================================================================
						break;
					}
				}
			}
		}
	}

	// 如果找了一圈发现还是 Identity (比如跨图时没找到对应的传送门，或者没有车票)
	if (SpawnTransform.Equals(FTransform::Identity))
	{
		// 【无空壳且非跨图，或跨图失败兜底】 -> 兜底找初始出生点 (PlayerStart)
		if (AActor* StartSpot = FindPlayerStart(NewPlayer))
		{
			// ==================== 【撤回错误优化，恢复 3D 纵深】 ====================
			// 同样，绝对不能锁 Y 轴！直接取 PlayerStart 在 3D 空间里的真实坐标。
			SpawnTransform = FTransform(StartSpot->GetActorRotation(), StartSpot->GetActorLocation(), FVector::OneVector);
			// ==============================================================================
		}
	}

	// 强制物理降临！
	FActorSpawnParameters SpawnParams;
	// 即使有微小物理干涉也挤开生成，确保玩家必生
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	SpawnParams.Owner = NewPlayer;

	// 亲自调用 SpawnActor，生成时自带 SpawnTransform 的正确位置和 2.5D 朝向，绝不外加 SetControlRotation！
	if (APawn* NewBody = GetWorld()->SpawnActor<APawn>(ClassToSpawn, SpawnTransform, SpawnParams))
	{
		NewPlayer->Possess(NewBody);
		// 闭环底层网络管线，同步正确的初始朝向给服务器
		FinishRestartPlayer(NewPlayer, SpawnTransform.GetRotation().Rotator());
		UE_LOG(LogTemp, Warning, TEXT("🚀 [动态捏人管线] 成功为玩家动态生成专属新肉体！"));
	}
}

void AMyGameModeBase::Logout(AController* Exiting)
{
	if (Exiting)
	{
		if (ATopCharacter* Teammate = Cast<ATopCharacter>(Exiting->GetPawn()))
		{
			// 【极致加固】：查验游戏逻辑上是否已经死了
			if (!Teammate->IsActorBeingDestroyed())
			{
				Exiting->UnPossess();

				// ==============================================================================
				// 【预留区域：未来友军 AI 重新接管逻辑】
				// ==============================================================================
				/*
				FActorSpawnParameters SpawnParams;
				SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				if (AController* NewAI = GetWorld()->SpawnActor<AController>(AMyFriendlyAIController::StaticClass(), Teammate->GetActorTransform(), SpawnParams))
				{
					NewAI->Possess(Teammate);
					UE_LOG(LogTemp, Warning, TEXT("🤖 [GameMode] 玩家掉线，友军 AI 已重新接管存活躯壳: %s"), *Teammate->GetName());
				}
				*/
			}
		}
	}
	Super::Logout(Exiting);
}

ATopCharacter* AMyGameModeBase::FindInitialStartupShell()
{
	// 声明并初始化开荒空壳指针，用于接收场景原生角色
	ATopCharacter* FoundShell = nullptr;

	// 🚀 优化实装：O(N) 极速扫描。
	// 利用 TActorIterator 遍历当前世界中所有的 ATopCharacter 实体
	for (TActorIterator<ATopCharacter> It(GetWorld()); It; ++It)
	{
		// 提取当前遍历到的角色实体指针
		ATopCharacter* Character = *It;

		// 跳过空指针或正在播放销毁动画的残骸
		if (!Character || Character->IsActorBeingDestroyed()) continue;

		// 筛选出当前没有控制器附身的无主躯壳
		if (Character->GetController() == nullptr)
		{
			// 【引擎底层修复】：针对 World Partition 流送机制的身份校正
			// RF_WasLoaded: 只要是从地图文件里读出来的，就一定有这个标号 (Map-placed)
			// !RF_Transient: 确保它不是运行时产生的临时垃圾或预览对象
			if (Character->HasAnyFlags(RF_WasLoaded) && !Character->HasAnyFlags(RF_Transient))
			{
				if (FoundShell == nullptr)
				{
					// 如果是场景原生预设件，且我们还没找到空壳，则选定它作为夺舍目标
					FoundShell = Character;
				}
				else
				{
					// 【核心逻辑保护】：既然它是地图原生预设件，即便我们不附身，也绝对不准销毁它！
					// 这样就能保留住第二个、第三个假人，解决“假人消失”的 Bug。
					UE_LOG(LogTemp, Display, TEXT("🛡️ [GameMode] 保留额外的合法预设件: %s"), *Character->GetName());
				}
			}
			// 【注意】：此处彻底废除 else { Destroy(); } 的盲目清场逻辑！
			// 房主跨图动态生成的待命小队成员没有 RF_WasLoaded，既不会被客机夺舍，也不会被误杀。
		}
	}
	return FoundShell;
}

ATopCharacter* AMyGameModeBase::FindSquadTeammateShell(AController* NewPlayer)
{
	// 声明并初始化空壳指针，用于接收小队队友
	ATopCharacter* FoundShell = nullptr;

	// 🚀 优化实装：O(N) 极速扫描。
	for (TActorIterator<ATopCharacter> It(GetWorld()); It; ++It)
	{
		ATopCharacter* Character = *It;

		if (!Character || Character->IsActorBeingDestroyed()) continue;

		// 筛选出当前没有控制器附身的无主躯壳
		if (Character->GetController() == nullptr)
		{
			// 1. 副机连入时：房主动态生成的待命队友没有 RF_WasLoaded，但已注册进 FriendlyRoster 大名单！
			// 2. !RF_Transient: 确保它不是运行时产生的临时垃圾或预览对象
			if (!Character->HasAnyFlags(RF_Transient) && FriendlyRoster.Contains(Character))
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

#pragma endregion
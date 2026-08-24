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

#include "MapTravel/MyMapTravelSubsystem.h"

// 【新增】：引入纯净的全局玩家状态基类
#include "PlayerState/MyPlayerState.h"


// ==============================================================================
// 生命周期 (Lifecycle)
// ==============================================================================
#pragma region

AMyGameModeBase::AMyGameModeBase()
{
	// 【核心配置】：在构造函数中开启无缝旅行支持，允许跨地图动态无缝加载，消除黑屏与连接中断
	// 这样引擎在加载此类时 (CDO阶段)，就会将其刻入默认配置
	bUseSeamlessTravel = true;

	// ==============================================================================
	// 【架构级焊死】：强行指定本游戏的 PlayerState 必须是我们写好组件的 C++ 类！
	// 无论策划在蓝图里怎么配，或者忘配，只要继承了这个 GameMode，引擎必定生成 AMyPlayerState！
	// 彻底消灭因蓝图配置遗漏导致的 5秒 死锁！
	// ==============================================================================
	PlayerStateClass = AMyPlayerState::StaticClass();
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
			// 💥【源头情报】：立刻捕获返回值，确定本局到底是不是开荒！
			bIsInitialBoot = StreamingSub->ResolveStarterDataLayer(StarterDataLayer);

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

				// ==============================================================================
				// 💥 【核心解耦】：如果是开荒，立刻通知传送子系统做好专属的开荒防御配置！
				// ==============================================================================
				if (bIsInitialBoot)
				{
					if (UMyMapTravelSubsystem* TravelSub = World->GetSubsystem<UMyMapTravelSubsystem>())
					{
						TravelSub->ExecuteInitialBootSetup();
					}
				}
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

			GetWorld()->GetTimerManager().SetTimerForNextTick([WeakSaveSub]()
				{
					if (WeakSaveSub.IsValid())
					{
						// 此时世界绝对安全，大管家正式下场，给各个已就绪的 Actor 分发读档数据
						WeakSaveSub->HandlePendingLoad();
					}

					// 💥 修复：删除了原来在此处的 WeakGI->PendingTravelRoute = nullptr 早泄撕票逻辑！
					// 必须把车票留到物理阵型彻底落位、引擎 Ready 之后再撕！
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

	// ==============================================================================
	// 💥 【架构极致解耦】：全权移交！GameMode 彻底闭嘴，导演系统接管一切！
	// 不再需要在这里写任何 if-else 分支，系统内部会通过查询数据资产图鉴进行 O(1) 仲裁。
	// ==============================================================================
	if (UMyMapTravelSubsystem* TravelSub = GetWorld()->GetSubsystem<UMyMapTravelSubsystem>())
	{
		// 导演系统将根据地图相性、开荒状态等自动判定执行“原体夺舍”还是“动态捏人+排队”
		if (TravelSub->ExecuteDeploymentDirector(NewPlayer, this))
		{
			return;
		}
	}

	Super::RestartPlayer(NewPlayer);
}

void AMyGameModeBase::ExecuteFinishRestartPlayer(AController* NewPlayer, const FRotator& StartRotation)
{
	// 内部直接调用引擎受保护的 FinishRestartPlayer，完成镜头对齐等收尾工作
	FinishRestartPlayer(NewPlayer, StartRotation);
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

#pragma endregion
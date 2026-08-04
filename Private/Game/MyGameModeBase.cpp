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

void AMyGameModeBase::StartPlay()
{
	// GameMode 的 StartPlay 是所有 Actor 准备就绪、游戏正式开始的冲锋号
	Super::StartPlay();

	// 【核心时序控制：第一阶段 - 读档分发微操 (NextTick Deferral)】

	// 尝试获取当前游戏世界的全局实例 (GameInstance)，它是跨关卡存活的最高级别实体
	if (UGameInstance* GI = GetGameInstance())
	{
		// 从游戏实例中，精准拉取负责统筹所有存档逻辑的大管家 (MySaveSubsystem)
		if (UMySaveSubsystem* SaveSub = GI->GetSubsystem<UMySaveSubsystem>())
		{
			// 灾难规避：此时新关卡的物理世界虽然就绪，但有些 Actor 的 BeginPlay 可能还没跑完！
			// 核心机制：利用 SetTimerForNextTick，强行将内部的“数据物理注入”挂起到引擎下一帧执行。
			// 借此让引擎跑完当前帧的地图收尾工作，确保物理世界地基彻底稳固。
			GetWorld()->GetTimerManager().SetTimerForNextTick([SaveSub]()
				{
					// 极限防御锁：哪怕只推迟了 1 帧 (约 16ms)，多线程及 GC 环境下对象依然可能失效，异步回调必须带锁
					if (IsValid(SaveSub))
					{
						// 此时世界绝对安全，大管家正式下场，给各个已就绪的 Actor 分发读档数据
						SaveSub->HandlePendingLoad();
					}
				});
		}
	}


	// 【核心时序控制：第二阶段 - 后台基建任务预热 (Background Infrastructure Warm-up)】

	// 灾难规避：严禁在异步 Lambda 中直接捕获裸指针 [this]。若玩家在 2 秒内强退或切图，GameMode 将被 GC 强行销毁，引发 Fatal Error。
	// 规范解法：遵循 UE 5.8 内存安全规范，使用 TWeakObjectPtr 弱引用捕获 GameMode 自身 (this)
	TWeakObjectPtr<AMyGameModeBase> WeakThis(this);

	// 声明一个定时器句柄，用于在底层追踪并管理这个 2 秒的延时任务
	FTimerHandle SaveWarmupTimer;

	// 架构目的：错峰出行（CPU Peak Shaving），避开地图刚加载时渲染层构建和 Shader 编译的性能算力尖峰。
	// 设置一个 2 秒的后台定时器，将高耗时的 IO 预载作业平滑推迟
	// 匿名函数 (Lambda) 极度干净利落，不需要额外去 .h 里声明一个专门的预热函数
	GetWorld()->GetTimerManager().SetTimer(SaveWarmupTimer, [WeakThis]()
		{
			// 极其优雅的内存安全判定：静默查验 2 秒后的 GameMode (this) 是否依然存活，若世界毁灭则静默失效，绝不越界
			if (WeakThis.IsValid())
			{
				// 再次安全获取当前 GameMode 所在新世界的游戏实例
				if (UGameInstance* GI = WeakThis->GetGameInstance())
				{
					// 再次安全拉取存档大管家子系统
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
	// (如果你以后有特定的剧情道具需要保留，再由你自己决定是否写进这个 ActorList 里)
}

AActor* AMyGameModeBase::ChoosePlayerStart_Implementation(AController* Player)
{
	// 1. 全局遍历寻找当前世界中的“大一统接机点”
	for (TActorIterator<AMyUniversalDestination> It(GetWorld()); It; ++It)
	{
		if (AMyUniversalDestination* Dest = *It)
		{
			// 找到后，直接把它当成玩家的合法产房，强塞给引擎底层！
			// 服务器会瞬间在这里为连入的客机生成肉体 (BaseCharacter)
			return Dest;
		}
	}

	// 2. 极端兜底：如果连接机点都没放，只能走引擎默认老路
	return Super::ChoosePlayerStart_Implementation(Player);
}

#pragma endregion

// ==============================================================================
// 联机底层探针与接客管线 (Network Probes & Player Spawning)
// ==============================================================================
#pragma region

void AMyGameModeBase::PostLogin(APlayerController* NewPlayer)
{
	UE_LOG(LogTemp, Warning, TEXT("========================================================="));
	UE_LOG(LogTemp, Warning, TEXT("🟢 [GameMode探针 - 1步] 客机玩家正式完成连接！Controller: %s"), *GetNameSafe(NewPlayer));

	// 调用父类逻辑，底层会自动去调 RestartPlayer
	Super::PostLogin(NewPlayer);
}

void AMyGameModeBase::RestartPlayer(AController* NewPlayer)
{
	UE_LOG(LogTemp, Warning, TEXT("🟢 [GameMode探针 - 2步] 服务器开始为玩家制造肉体... Controller: %s"), *GetNameSafe(NewPlayer));

	Super::RestartPlayer(NewPlayer);

	// 验证肉体是否制造成功并完成附身
	if (NewPlayer->GetPawn())
	{
		UE_LOG(LogTemp, Warning, TEXT("✅ [GameMode探针 - 终点] 成功！肉体生成并附身完毕！Pawn: %s"), *GetNameSafe(NewPlayer->GetPawn()));
		UE_LOG(LogTemp, Warning, TEXT("========================================================="));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("❌ [GameMode探针 - 致命失败] RestartPlayer 执行完毕，但玩家仍然没有 Pawn！"));
		UE_LOG(LogTemp, Error, TEXT("========================================================="));
	}
}

AActor* AMyGameModeBase::FindPlayerStart_Implementation(AController* Player, const FString& IncomingName)
{
	UE_LOG(LogTemp, Warning, TEXT("🟡 [GameMode探针 - 3步] 准备开始寻找出生点..."));

	// 调用父类逻辑，它底层会自动去调用上面无缝旅行分区里的 ChoosePlayerStart_Implementation
	AActor* StartSpot = Super::FindPlayerStart_Implementation(Player, IncomingName);

	if (StartSpot)
	{
		UE_LOG(LogTemp, Warning, TEXT("🟡 [GameMode探针 - 3步] 寻找完毕！最终确定的出生点是: %s"), *StartSpot->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("🔴 [GameMode探针 - 3步致命错误] 未能找到任何合法出生点！"));
	}

	return StartSpot;
}

APawn* AMyGameModeBase::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	UE_LOG(LogTemp, Warning, TEXT("🟡 [GameMode探针 - 4步] 准备在坐标 %s 物理生成肉体..."), *SpawnTransform.GetLocation().ToString());

	// 查看 GameMode 到底有没有配置默认肉体类
	if (!DefaultPawnClass)
	{
		UE_LOG(LogTemp, Error, TEXT("🔴 [GameMode探针 - 4步致命错误] GameMode 的 DefaultPawnClass 是空的！请检查世界设置或蓝图配置！"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("🟡 [GameMode探针 - 4步] 即将生成的肉体类型是: %s"), *DefaultPawnClass->GetName());
	}

	// 执行底层生成 (注意：Super 调用也必须加 _Implementation)
	APawn* SpawnedPawn = Super::SpawnDefaultPawnAtTransform_Implementation(NewPlayer, SpawnTransform);

	if (!SpawnedPawn)
	{
		UE_LOG(LogTemp, Error, TEXT("🔴 [GameMode探针 - 4步致命错误] 肉体物理生成失败！可能是发生严重物理碰撞，或蓝图类损坏！"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("🟡 [GameMode探针 - 4步] 肉体实体已成功诞生于世界中！"));
	}

	return SpawnedPawn;
}

#pragma endregion
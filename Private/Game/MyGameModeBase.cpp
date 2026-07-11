// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/MyGameModeBase.h"
// 角色
#include "Character/TopCharacter.h"

// 引入后台预热所需的底层系统头文件
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"
#include "SaveGame/Subsystem/MySaveSubsystem.h"

// 无缝流转所需的头文件
#include "EngineUtils.h"
#include "Weapon/Projectile/MyBaseProjectile.h"

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

		/*
		// 2. 遍历清理所有控制器的残留异步任务
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (APlayerController* PC = It->Get())
			{
				World->GetTimerManager().ClearAllTimersForObject(PC);
			}
		}
		*/
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

#pragma endregion
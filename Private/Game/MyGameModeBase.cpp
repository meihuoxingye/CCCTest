// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/MyGameModeBase.h"
// 角色
#include "Character/TopCharacter.h"

// 引入后台预热所需的底层系统头文件
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"
#include "SaveGame/Subsystem/MySaveSubsystem.h"


// ==============================================================================
// 生命周期 (Lifecycle)
// ==============================================================================
#pragma region

void AMyGameModeBase::StartPlay()
{
	// GameMode 的 StartPlay 是所有 Actor 准备就绪、游戏正式开始的冲锋号
	Super::StartPlay();

	// ==============================================================================
	// 【新增：跨关卡读档落地执行】
	// 此时新关卡的物理世界和 Actor 已完全存活，唤醒大管家去执行挂起的读档恢复任务
	// ==============================================================================
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMySaveSubsystem* SaveSub = GI->GetSubsystem<UMySaveSubsystem>())
		{
			SaveSub->HandlePendingLoad();
		}
	}


	// ==============================================================================
	// 后台基建任务预热 (Background Infrastructure Warm-up)
	// ==============================================================================

	// 使用弱引用捕获 'this'，防止在 2 秒内切换关卡导致的崩溃风险 (UE5.7 规范)
	TWeakObjectPtr<AMyGameModeBase> WeakThis(this);

	// 设置一个 2 秒的后台定时器，避开加载地图初期的 CPU 性能峰值
	FTimerHandle SaveWarmupTimer;
	GetWorld()->GetTimerManager().SetTimer(SaveWarmupTimer, [WeakThis]()
		{
			// 匿名函数 (Lambda) 极度干净利落，不需要额外去 .h 里声明一个专门的预热函数
			// 检查 GameMode 是否依然存活
			if (WeakThis.IsValid())
			{
				if (UGameInstance* GI = WeakThis->GetGameInstance())
				{
					if (UMySaveSubsystem* SaveSub = GI->GetSubsystem<UMySaveSubsystem>())
					{
						// 避开 CPU 负载尖峰，平稳执行异步预加载
						SaveSub->PreloadRegistry();
					}
				}
			}
		}, 2.0f, false);
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
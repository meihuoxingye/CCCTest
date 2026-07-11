// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Transition/MyTransitionWidgetBase.h"
#include "Game/MyGameInstance.h"
#include "Kismet/GameplayStatics.h"

// ==============================================================================
// 外部大管家契约接口 (External Master Contracts)
// ==============================================================================
#pragma region

void UMyTransitionWidgetBase::NotifyEngineReady()
{
	// 只有在处于“挂起等待”状态时，才接受大管家的退场指令
	if (bIsWaitingForEngine)
	{
		bIsWaitingForEngine = false;
		// 呼叫电池：开始跑退场动画！（进度从 1.0 往 0.0 减）
		AnimModule.StartClosing();
	}
}

#pragma endregion


// ==============================================================================
// 动画渲染驱动源 (Animation Drive Source)
// ==============================================================================
// (纯蓝图钩子，无需在 C++ 实现)


// ==============================================================================
// 控件生命周期 (Widget Lifecycle)
// ==============================================================================
#pragma region

void UMyTransitionWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	// 防止编辑器预览模式执行运行时逻辑
	if (IsDesignTime()) return;

	bIsWaitingForEngine = false;

	// UI 实例化上屏的第一帧，直接命令电池开始跑入场动画
	AnimModule.StartOpening();
}

void UMyTransitionWidgetBase::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	// 【编辑器实时预览】：完美复用电池里的逻辑
	if (AnimModule.CurrentState == EUIState::Opening || AnimModule.CurrentState == EUIState::Idle)
	{
		// 你在蓝图编辑器里拖动 TransitionProgress 滑块，画面就会动！
		BP_UpdateIntroEffect(AnimModule.TransitionProgress, AnimModule.GetEasedProgress());
	}
	else if (AnimModule.CurrentState == EUIState::Closing)
	{
		BP_UpdateOutroEffect(AnimModule.TransitionProgress, AnimModule.GetEasedProgress());
	}
}

void UMyTransitionWidgetBase::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 编辑器模式，或者正在死等引擎的状态下，切断 Tick 节省开销
	if (IsDesignTime() || bIsWaitingForEngine) return;

	// 1. 【电池驱动】：把 DeltaTime 喂给电池，让它自己去算那些复杂的除法和数学逻辑
	// 返回值为 true 代表动画在本帧刚好彻底播完
	bool bJustFinished = AnimModule.Tick(InDeltaTime);

	// 2. 【分流渲染】：根据电池的当前状态，或者刚刚结束的状态，去调用蓝图钩子
	if (AnimModule.CurrentState == EUIState::Opening || (bJustFinished && AnimModule.TransitionProgress >= 1.0f))
	{
		// 把电池算好的结果直接抛给蓝图
		BP_UpdateIntroEffect(AnimModule.TransitionProgress, AnimModule.GetEasedProgress());

		if (bJustFinished)
		{
			// 入场播完，上锁！进入静默挂起模式，等待 GameInstance 的唤醒
			bIsWaitingForEngine = true;
		}
	}
	else if (AnimModule.CurrentState == EUIState::Closing || (bJustFinished && AnimModule.TransitionProgress <= 0.0f))
	{
		BP_UpdateOutroEffect(AnimModule.TransitionProgress, AnimModule.GetEasedProgress());

		if (bJustFinished)
		{
			// 【终极收尾】：退场动画彻底播完（擦干净了）
			// 电池已经把进度降到了 0.0，UI 自我了断，反向呼叫大管家进行物理粉碎！
			if (UMyGameInstance* GI = Cast<UMyGameInstance>(UGameplayStatics::GetGameInstance(this)))
			{
				GI->FinalizeLoadingScreenRemoval();
			}
		}
	}
}

#pragma endregion
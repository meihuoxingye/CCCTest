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

void UMyTransitionWidgetBase::SetTransitionDuration(float InIntroDuration)
{
	// 赋给它什么值，电池就跑多少秒，绝不逼逼！
	if (InIntroDuration > 0.0f)
	{
		AnimModule.OpeningDuration = InIntroDuration;
	}
}

#pragma endregion


// ==============================================================================
// 控件生命周期 (Widget Lifecycle)
// ==============================================================================
#pragma region

void UMyTransitionWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	// 防止编辑器预览模式执行运行时逻辑
	if (IsDesignTime()) return;

	// 【防呆文本物理着陆】：硬编码锁死警告内容
	ArchitectureWarning = NSLOCTEXT("Architecture", "TransitionWarning",
		"【系统时序契约】：\n"
		"1. 此处的 Opening/Closing Duration 在本类中仅供【编辑器视口拖动预览】！\n"
		"2. 游戏运行时，此处的具体秒数将被完全废弃，系统将强行注入关卡触发器(Trigger)中配置的物理时间！\n"
		"3. 美术仅需在此类中调节缓动指数(Exp)和视觉效果，绝对不要和策划硬刚运行时时间！");

	bIsWaitingForEngine = false;

	// UI 实例化上屏的第一帧，直接命令电池开始跑入场动画
	AnimModule.StartOpening();
}

void UMyTransitionWidgetBase::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	// 保证编辑器模式下，美术一编译或者一改参数，警告文本就能在细节面板里被刷成灰色
	ArchitectureWarning = NSLOCTEXT("Architecture", "TransitionWarning",
		"【系统时序契约】：\n"
		"1. 此处的 Opening/Closing Duration 在本类中仅供【编辑器视口拖动预览】！\n"
		"2. 游戏运行时，此处的具体秒数将被完全废弃，系统将强行注入关卡触发器(Trigger)中配置的物理时间！\n"
		"3. 美术仅需在此类中调节缓动指数(Exp)和视觉效果，绝对不要和策划硬刚运行时时间！");

	// 【编辑器实时预览】：完美复用电池里的逻辑
	if (AnimModule.CurrentState == EUIAnimationState::Opening || AnimModule.CurrentState == EUIAnimationState::Idle)
	{
		UpdateOpeningEffect(AnimModule.TransitionProgress, AnimModule.GetEasedProgress());
	}
	else if (AnimModule.CurrentState == EUIAnimationState::Closing)
	{
		UpdateClosingEffect(AnimModule.TransitionProgress, AnimModule.GetEasedProgress());
	}
}

void UMyTransitionWidgetBase::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (IsDesignTime() || bIsWaitingForEngine) return;

	bool bJustFinished = AnimModule.Tick(InDeltaTime);

	// 【修改这里】：全换成 EUIAnimationState
	if (AnimModule.CurrentState == EUIAnimationState::Opening || (bJustFinished && AnimModule.TransitionProgress >= 1.0f))
	{
		UpdateOpeningEffect(AnimModule.TransitionProgress, AnimModule.GetEasedProgress());

		if (bJustFinished)
		{
			bIsWaitingForEngine = true;
		}
	}
	else if (AnimModule.CurrentState == EUIAnimationState::Closing || (bJustFinished && AnimModule.TransitionProgress <= 0.0f))
	{
		UpdateClosingEffect(AnimModule.TransitionProgress, AnimModule.GetEasedProgress());

		if (bJustFinished)
		{
			if (UMyGameInstance* GI = Cast<UMyGameInstance>(UGameplayStatics::GetGameInstance(this)))
			{
				GI->FinalizeLoadingScreenRemoval();
			}
		}
	}
}

#pragma endregion
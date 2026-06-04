// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/ActivatableWidget/MyActivatableWidgetBase.h"
#include "Kismet/GameplayStatics.h"
#include "Input/Reply.h"
#include "Input/Events.h"

// ==============================================================================
// 生命周期与同步 (Lifecycle & Synchronization)
// ==============================================================================
#pragma region
void UMyActivatableWidgetBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 强行抹除编辑器残留，绝对防止第一次呼出时引发瞬移
	TransitionProgress = 0.0f;
	CurrentState = EUIState::Idle;
}

void UMyActivatableWidgetBase::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	// 编辑器实时预览：根据当前选择的状态，分流测试对应的表现钩子
	if (CurrentState == EUIState::Opening || CurrentState == EUIState::Idle)
	{
		float EasedProgress = FMath::InterpEaseInOut(0.0f, 1.0f, TransitionProgress, OpeningExp);
		UpdateOpeningEffect(TransitionProgress, EasedProgress);
	}
	else if (CurrentState == EUIState::Closing)
	{
		float EasedProgress = FMath::InterpEaseInOut(0.0f, 1.0f, TransitionProgress, ClosingExp);
		UpdateClosingEffect(TransitionProgress, EasedProgress);
	}
}

void UMyActivatableWidgetBase::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (CurrentState == EUIState::Idle) return;

	// 彻底移除 Dilation，直接使用原生 InDeltaTime，杜绝任何倍速跳帧现象
	float CurrentDuration = (CurrentState == EUIState::Opening) ? OpeningDuration : ClosingDuration;
	float Step = InDeltaTime / FMath::Max(0.01f, CurrentDuration);

	if (CurrentState == EUIState::Opening)
	{
		TransitionProgress = FMath::Min(TransitionProgress + Step, 1.0f);
	}
	else if (CurrentState == EUIState::Closing)
	{
		TransitionProgress = FMath::Max(TransitionProgress - Step, 0.0f);
	}

	// 进度分流演算与调用
	if (CurrentState == EUIState::Opening)
	{
		float EasedProgress = FMath::InterpEaseInOut(0.0f, 1.0f, TransitionProgress, OpeningExp);
		UpdateOpeningEffect(TransitionProgress, EasedProgress);

		// 状态机收尾
		if (TransitionProgress >= 1.0f) CurrentState = EUIState::Idle;
	}
	else if (CurrentState == EUIState::Closing)
	{
		float EasedProgress = FMath::InterpEaseInOut(0.0f, 1.0f, TransitionProgress, ClosingExp);
		UpdateClosingEffect(TransitionProgress, EasedProgress);

		// 状态机收尾
		if (TransitionProgress <= 0.0f)
		{
			CurrentState = EUIState::Idle;
			SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}
#pragma endregion

// ==============================================================================
// 响应式输入路由 (Input Routing)
// ==============================================================================
#pragma region
FReply UMyActivatableWidgetBase::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		// 允许在“完全可见” 或 “正在出场” 的半空中发送关闭信号，实现丝滑打断
		if (CurrentState == EUIState::Idle || CurrentState == EUIState::Opening)
		{
			OnCloseRequested.Broadcast();
		}

		return FReply::Handled();
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton ||
		InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		return FReply::Unhandled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UMyActivatableWidgetBase::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Tab || InKeyEvent.GetKey() == EKeys::Escape)
	{
		return FReply::Unhandled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
#pragma endregion

// ==============================================================================
// 状态驱动接口 (State Drivers)
// ==============================================================================
#pragma region
void UMyActivatableWidgetBase::OnWidgetActivated_Implementation()
{
	// 防误触：已经在飞入中，坚决拦截
	if (CurrentState == EUIState::Opening) return;

	// 丝滑打断算法：完全关着才重置起点。中途呼出时必须保留当前进度直接掉头。
	if (GetVisibility() == ESlateVisibility::Collapsed || TransitionProgress <= 0.0f)
	{
		TransitionProgress = 0.0f;
	}

	SetVisibility(ESlateVisibility::Visible);
	CurrentState = EUIState::Opening;

	// 第一帧抗闪现防护
	float EasedProgress = FMath::InterpEaseInOut(0.0f, 1.0f, TransitionProgress, OpeningExp);
	UpdateOpeningEffect(TransitionProgress, EasedProgress);
}

void UMyActivatableWidgetBase::OnWidgetDeactivated_Implementation()
{
	// 防误触：已经在收起或不可见，坚决拦截
	if (CurrentState == EUIState::Closing || GetVisibility() == ESlateVisibility::Collapsed) return;

	CurrentState = EUIState::Closing;

	// 丝滑打断算法：绝不重置 TransitionProgress 到 1.0f。
	// 从当前被截断的数字直接往下扣，交由底层 Tick 和无坐标赋值的蓝图实现原地坠落。
}
#pragma endregion
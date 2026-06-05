// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/ActivatableWidget/MyActivatableWidgetBase.h"
#include "Kismet/GameplayStatics.h"
#include "Input/Reply.h"
#include "Input/Events.h"

// 【新增】：为了获取 LocalPlayer 和 UI 子系统
#include "Engine/LocalPlayer.h"
#include "UI/Subsystem/MyUIManagerSubsystem.h"

// ==============================================================================
// 生命周期与同步 (Lifecycle & Synchronization)
// ==============================================================================
#pragma region
void UMyActivatableWidgetBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	TransitionProgress = 0.0f;
	CurrentState = EUIState::Idle;
}

void UMyActivatableWidgetBase::SynchronizeProperties()
{
	Super::SynchronizeProperties();

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

	if (CurrentState == EUIState::Opening)
	{
		float EasedProgress = FMath::InterpEaseInOut(0.0f, 1.0f, TransitionProgress, OpeningExp);
		UpdateOpeningEffect(TransitionProgress, EasedProgress);

		if (TransitionProgress >= 1.0f) CurrentState = EUIState::Idle;
	}
	else if (CurrentState == EUIState::Closing)
	{
		float EasedProgress = FMath::InterpEaseInOut(0.0f, 1.0f, TransitionProgress, ClosingExp);
		UpdateClosingEffect(TransitionProgress, EasedProgress);

		// 状态机收尾：当进度彻底归零（退场动画播完）
		if (TransitionProgress <= 0.0f)
		{
			CurrentState = EUIState::Idle;
			SetVisibility(ESlateVisibility::Collapsed);

			// 【核心修复】：直到 UI 彻底看不见了，才将其出栈！
			// 这彻底封死了“退场动画期间开火走火”的幽灵点击 Bug。
			if (ULocalPlayer* LP = GetOwningLocalPlayer())
			{
				if (UMyUIManagerSubsystem* UIMgr = LP->GetSubsystem<UMyUIManagerSubsystem>())
				{
					UIMgr->PopUI(this);
				}
			}
		}
	}
}

void UMyActivatableWidgetBase::NativeDestruct()
{
	// 最后的保险：无论 UI 是怎么没的（切场景、被强杀等），都要从子系统中消除
	if (ULocalPlayer* LP = GetOwningLocalPlayer())
	{
		if (UMyUIManagerSubsystem* UIMgr = LP->GetSubsystem<UMyUIManagerSubsystem>())
		{
			UIMgr->PopUI(this);
		}
	}

	Super::NativeDestruct();
}
#pragma endregion

// ==============================================================================
// 响应式输入路由 (Input Routing)
// ==============================================================================
#pragma region
FReply UMyActivatableWidgetBase::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
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
	if (CurrentState == EUIState::Opening) return;

	if (GetVisibility() == ESlateVisibility::Collapsed || TransitionProgress <= 0.0f)
	{
		TransitionProgress = 0.0f;
	}

	SetVisibility(ESlateVisibility::Visible);
	CurrentState = EUIState::Opening;

	// 【全自动入栈】
	if (ULocalPlayer* LP = GetOwningLocalPlayer())
	{
		if (UMyUIManagerSubsystem* UIMgr = LP->GetSubsystem<UMyUIManagerSubsystem>())
		{
			UIMgr->PushUI(this);
		}
	}

	float EasedProgress = FMath::InterpEaseInOut(0.0f, 1.0f, TransitionProgress, OpeningExp);
	UpdateOpeningEffect(TransitionProgress, EasedProgress);
}

void UMyActivatableWidgetBase::OnWidgetDeactivated_Implementation()
{
	if (CurrentState == EUIState::Closing || GetVisibility() == ESlateVisibility::Collapsed) return;

	CurrentState = EUIState::Closing;

	// 注意：这里绝不调用 PopUI！留在拦截栈中抗点击穿透，交由 Tick 动画结束时出栈。
}
#pragma endregion
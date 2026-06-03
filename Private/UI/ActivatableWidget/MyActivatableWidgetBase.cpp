#include "UI/ActivatableWidget/MyActivatableWidgetBase.h"
#include "Animation/WidgetAnimation.h"
#include "Kismet/GameplayStatics.h"
#include "Input/Reply.h"
#include "Input/Events.h"

void UMyActivatableWidgetBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();
}

void UMyActivatableWidgetBase::NativeDestruct()
{
	Super::NativeDestruct();
}

void UMyActivatableWidgetBase::PlayCompensatedAnimation(UWidgetAnimation* InAnimation, EUMGSequencePlayMode::Type PlayMode)
{
	if (!InAnimation) return;

	float PlaySpeed = 1.0f;
	if (UWorld* World = GetWorld())
	{
		float Dilation = UGameplayStatics::GetGlobalTimeDilation(World);
		PlaySpeed = 1.0f / FMath::Max(0.01f, Dilation);
	}

	PlayAnimation(InAnimation, 0.0f, 1, PlayMode, PlaySpeed);
}

// ==============================================================================
// 响应式输入路由 (Input Routing)
// ==============================================================================
FReply UMyActivatableWidgetBase::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 【防点穿】：左键点击 UI 背景，拦截信号并向外界广播关闭请求
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		OnCloseRequested.Broadcast();
		return FReply::Handled();
	}

	// 【防死锁】：放行鼠标中键（或右键），让信号穿过 UI 树抵达底层控制器
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

// ==============================================================================
// 状态自适应管理
// ==============================================================================
void UMyActivatableWidgetBase::OnWidgetActivated_Implementation()
{
	SetVisibility(ESlateVisibility::Visible);

	if (AppearAnim)
	{
		PlayCompensatedAnimation(AppearAnim, EUMGSequencePlayMode::Forward);
	}
}

void UMyActivatableWidgetBase::OnWidgetDeactivated_Implementation()
{
	if (GetVisibility() == ESlateVisibility::Collapsed) return;

	if (DisappearAnim)
	{
		PlayCompensatedAnimation(DisappearAnim, EUMGSequencePlayMode::Forward);
	}
	else
	{
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UMyActivatableWidgetBase::OnAnimationFinished_Implementation(const UWidgetAnimation* Animation)
{
	Super::OnAnimationFinished_Implementation(Animation);

	if (Animation == DisappearAnim)
	{
		SetVisibility(ESlateVisibility::Collapsed);
	}
}
// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Subsystem/MyUIManagerSubsystem.h"
#include "UI/ActivatableWidget/MyActivatableWidgetBase.h"
#include "Tools/MyUITools.h"

// ==============================================================================
// 栈操作与生命周期 (Stack Operations)
// ==============================================================================
#pragma region
void UMyUIManagerSubsystem::PushUI(UMyActivatableWidgetBase* Widget)
{
	if (IsValid(Widget))
	{
		ActiveModalUIs.AddUnique(Widget);
	}
}

void UMyUIManagerSubsystem::PopUI(UMyActivatableWidgetBase* Widget)
{
	if (IsValid(Widget))
	{
		ActiveModalUIs.Remove(Widget);
	}
}
#pragma endregion

// ==============================================================================
// 全局交互仲裁 (Global Interaction Arbitration)
// ==============================================================================
#pragma region
bool UMyUIManagerSubsystem::TryConsumeClick()
{
	ActiveModalUIs.RemoveAll([](const TObjectPtr<UMyActivatableWidgetBase>& W) {
		return !IsValid(W);
		});

	if (UMyUITools::IsMouseOverUI(this))
	{
		return true;
	}

	if (ActiveModalUIs.Num() > 0)
	{
		UMyActivatableWidgetBase* TopUI = ActiveModalUIs.Last();

		if (IsValid(TopUI))
		{
			if (TopUI->GetCurrentState() != EUIState::Closing)
			{
				TopUI->OnCloseRequested.Broadcast();
			}

			return true;
		}
	}

	return false;
}
#pragma endregion
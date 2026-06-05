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
	// 1. 清理失效指针
	ActiveModalUIs.RemoveAll([](const TObjectPtr<UMyActivatableWidgetBase>& W) {
		return !IsValid(W);
		});

	// ==============================================================================
	// 2. 【核心颠覆：优先判定 UI 栈！】
	// 如果代码走到了这里，说明玩家这一下鼠标【绝对没有】点在 UI 的实体按钮上
	// 所以，只要栈里有弹窗开着，说明玩家百分百点在了空白处！立刻关闭！
	// ==============================================================================
	if (ActiveModalUIs.Num() > 0)
	{
		UMyActivatableWidgetBase* TopUI = ActiveModalUIs.Last();

		if (IsValid(TopUI))
		{
			// 防幽灵点击：如果还没处于关闭状态，就发广播关掉它
			if (TopUI->GetCurrentState() != EUIState::Closing)
			{
				TopUI->OnCloseRequested.Broadcast();
			}

			// 告诉角色：这一下点击我已经用来关 UI 了，你给我憋着，绝不允许开枪！
			return true;
		}
	}

	// ==============================================================================
	// 3. 【兜底防误触：最后才用全局射线】
	// 只有在【没有任何弹窗开着】的时候，才用全局射线检测是不是点到了 HUD 的血条上。
	// ==============================================================================
	if (UMyUITools::IsMouseOverUI(this))
	{
		return true; // 点到了常驻 HUD 上，吸收点击，防走火
	}

	// 4. 什么 UI 都没开，且点在了纯正的 3D 世界上 -> 放行，允许角色开枪！
	return false;
}
#pragma endregion
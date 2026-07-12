// Fill out your copyright notice in the Description page of Project Settings.

// 引入 UI 管理子系统的头文件
#include "UI/Subsystem/MyUIManagerSubsystem.h"
// 引入需要统筹调度的基础 UI 控件头文件
#include "UI/ActivatableWidget/MyActivatableWidgetBase.h"
// 引入自定义 UI 工具函数库（比如用于射线检测/鼠标悬停检测等）
#include "Tools/MyUITools.h"
// 【引入我们的动画模块电池】：解耦纯数学驱动逻辑
#include "UI/MyUIAnimationModule.h"

// ==============================================================================
// 栈操作与生命周期 (Stack Operations)
// ==============================================================================
#pragma region

void UMyUIManagerSubsystem::PushUI(UMyActivatableWidgetBase* Widget)
{
	// 安全校验：确保传入的 Widget 指针非空且尚未被引擎标记为等待销毁 (Pending Kill)
	if (IsValid(Widget))
	{
		// 将该 UI 压入拦截栈。AddUnique 保证即使误调用，同一个 UI 实例也不会在栈中出现两次
		ActiveModalUIs.AddUnique(Widget);
	}
}

void UMyUIManagerSubsystem::PopUI(UMyActivatableWidgetBase* Widget)
{
	// 同样进行安全校验
	if (IsValid(Widget))
	{
		// 从活跃的模态 UI 数组栈中移除指定的面板实例
		ActiveModalUIs.Remove(Widget);
	}
}

#pragma endregion

// ==============================================================================
// 全局交互仲裁 (Global Interaction Arbitration)
// ==============================================================================
#pragma region

bool UMyUIManagerSubsystem::ProcessUIClick()
{
	// 【内存安全防线】：在判定前，先利用 Lambda 表达式强行扫除栈中所有已失效（如已被垃圾回收）的悬空指针
	ActiveModalUIs.RemoveAll([](const TObjectPtr<UMyActivatableWidgetBase>& W) 
		{
			return !IsValid(W); // 返回 true 意味着 "移除该无效元素"
		});

	// 调用工具库，检查当前光标是否停留在任何 UMG UI 几何体上面
	if (UMyUITools::IsMouseOverUI(this))
	{
		// 既然点在了 UI 界面上，无论是什么 UI，直接宣告本次点击被“消化”掉，防止穿透到背后的 3D 游戏世界
		return true;
	}

	// 检查当前管理者栈内是否有登记在案的激活 UI 
	if (ActiveModalUIs.Num() > 0)
	{
		// 获取位于数组最末尾（也就是显示层级最高、栈顶部位）的 UI 控件
		UMyActivatableWidgetBase* TopUI = ActiveModalUIs.Last();

		// 确认取出的栈顶指针依然处于有效状态
		if (IsValid(TopUI))
		{
			// 如果这个栈顶 UI 的状态机不等于 "正在关闭" （即处于展开中或静止展开完毕状态）
			if (TopUI->GetCurrentState() != EUIAnimationState::Closing)
			{
				// 【核心新增逻辑】：查阅该 UI 的档案，看它是否允许被背景点击强制关闭
				if (TopUI->bCanBeClosedByBackgroundClick)
				{
					// 触发该面板的关闭请求信号，通知其自身或蓝图进行退场收尾工作
					TopUI->OnCloseRequested.Broadcast();
				}
			}

			// 只要栈里还有激活面板，这次在背景的点击就予以拦截，并关闭当前顶级菜单的交互
			return true;
		}
	}

	// 如果鼠标没有点在任何 UI 元素上，且栈里也为空（没有开启任何面板）
	// 则返回 false，放行本次输入，让背后的玩家角色正常开火/互动
	return false;
}

#pragma endregion
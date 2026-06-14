// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "MyUIManagerSubsystem.generated.h"


/**
 * 本地玩家 UI 统筹子系统 (Local Player UI Manager)
 * 职责：全权管理当前玩家屏幕上的 UI 栈，负责弹窗拦截、优先级与物理点击判定。
 */
UCLASS()
class CCC_API UMyUIManagerSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

	// ==============================================================================
	// UI 栈数据结构 (UI Stack Data)
	// ==============================================================================
private:
	// 【内存安全升级】：使用 UE5 推荐的 TObjectPtr 替代原始指针，配合 IsValid 进行安全校验
	// 维护一个动态数组，作为管理当前激活 UI 实例的“栈”
	UPROPERTY()
	TArray<TObjectPtr<class UMyActivatableWidgetBase>> ActiveModalUIs;


	// ==============================================================================
	// 栈操作与生命周期 (Stack Operations)
	// ==============================================================================
public:
	// 将一个激活的 UI 面板压入管理栈
	// UMyActivatableWidgetBase::OnWidgetActivated()调用
	void PushUI(UMyActivatableWidgetBase* Widget);

	// 从管理栈中弹出一个已关闭或销毁的 UI 面板
	// 【严格调用时机 - 双重保险】：
	// 1. 正常休眠 (NativeTick)：退场动画彻底播完 (TransitionProgress <= 0.0f) 且状态切为 Collapsed 时（适用于常驻缓存 UI）。
	// 2. 物理抹除 (NativeDestruct)：UI 被 RemoveFromParent 失去强引用，或切换关卡导致世界毁灭时触发。
	// 【致命兜底】：若 UI 未播完动画就被强制销毁，NativeTick 会被瞬间腰斩，无法走到休眠逻辑；
	//	此时必须依靠 NativeDestruct 强制出栈，否则管理器将持有野指针导致输入崩溃！
	// 【架构警告】：严禁在 UI 刚开始退场（如 OnWidgetDeactivated）时调用！
	// 必须保持入栈状态直到退场结束或被物理抹除，否则会导致退场期间的“穿透点击/走火”Bug。
	void PopUI(UMyActivatableWidgetBase* Widget);


	// ==============================================================================
	// 全局交互仲裁 (Global Interaction Arbitration)
	// ==============================================================================
public:
	// 供全局控制器调用的 UI 专属点击事件处理器。
	// 活动面板的第二道防线，NativeOnMouseButtonDown 会拦截点击到 UI 的不能放行鼠标按键
	// 其他可放行的点击 UI 事件以及未点到 UI 的事件将会经过这里的判决
	// 1. 【清扫】：自动清理 UI 管理栈中已失效的悬空指针。
	// 2. 【消耗】：判定点击是否落在 UI 物理几何体内，若是则消耗该输入。
	// 3. 【关闭】：处理特定的 UI 交互逻辑（如点击空白处关闭最上层模态面板）。
	bool ProcessUIClick();
};
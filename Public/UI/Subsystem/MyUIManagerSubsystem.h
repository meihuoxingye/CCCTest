// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "MyUIManagerSubsystem.generated.h"

class UMyActivatableWidgetBase;

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
	UPROPERTY()
	TArray<TObjectPtr<UMyActivatableWidgetBase>> ActiveModalUIs;


	// ==============================================================================
	// 栈操作与生命周期 (Stack Operations)
	// ==============================================================================
public:
	void PushUI(UMyActivatableWidgetBase* Widget);

	void PopUI(UMyActivatableWidgetBase* Widget);


	// ==============================================================================
	// 全局交互仲裁 (Global Interaction Arbitration)
	// ==============================================================================
public:
	// 供外部（如 Controller 或 Character）调用的终极接口。
	bool TryConsumeClick();
};
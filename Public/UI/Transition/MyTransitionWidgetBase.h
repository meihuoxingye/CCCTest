// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
// 【引入我们的动画模块电池】
#include "UI/MyUIAnimationModule.h" 
#include "MyTransitionWidgetBase.generated.h"

/**
 * 所有转场相关 UI（熄屏、过场黑幕、加载屏）的统一下层绝对基类。
 * 通过装配 FMyUIAnimationModule 获取了 3A 级的动画驱动能力。
 * 【注】：本类绝不包含进度条或具体业务数据，需要进度条的 UI 请继承此类实现。
 */
UCLASS(Abstract, Blueprintable)
class CCC_API UMyTransitionWidgetBase : public UUserWidget
{
	GENERATED_BODY()

	// ==============================================================================
	// 外部大管家契约接口 (External Master Contracts)
	// ==============================================================================
public:
	// 大管家呼叫：底层死锁结束，新世界准备就绪，可以开始播退场动画了！
	UFUNCTION(BlueprintCallable, Category = "TransitionUI")
	virtual void NotifyEngineReady();


	// ==============================================================================
	// 动画渲染驱动源 (Animation Drive Source)
	// ==============================================================================
protected:
	// 【核心装配】：将动画模块作为电池嵌入！
	// 奇迹宏 ShowOnlyInnerProperties 会让这个结构体在细节面板中隐形，
	// 里面的 OpeningDuration 等参数会直接平铺在 UI 的外层，美术体验极佳！
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TransitionUI|Animation", meta = (ShowOnlyInnerProperties))
	FMyUIAnimationModule AnimModule;

	// 【挂起状态锁】：用于入场播完后，安静等待大管家发令
	UPROPERTY(BlueprintReadOnly, Category = "TransitionUI|State")
	bool bIsWaitingForEngine = false;

	// 【入场动画渲染钩子】
	// @param Progress 线性进度 (0.0 到 1.0)
	// @param EasedProgress 经过缓动曲线平滑后的视觉进度
	UFUNCTION(BlueprintImplementableEvent, Category = "TransitionUI|Animation")
	void BP_UpdateIntroEffect(float Progress, float EasedProgress);

	// 【退场动画渲染钩子】
	// @param Progress 线性进度 (1.0 递减到 0.0)
	// @param EasedProgress 经过缓动曲线平滑后的视觉进度
	UFUNCTION(BlueprintImplementableEvent, Category = "TransitionUI|Animation")
	void BP_UpdateOutroEffect(float Progress, float EasedProgress);


	// ==============================================================================
	// 控件生命周期 (Widget Lifecycle)
	// ==============================================================================
protected:
	virtual void NativeConstruct() override;

	// 【所见即所得】：编辑器与 C++ 数据同步桥梁。
	virtual void SynchronizeProperties() override;

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
};
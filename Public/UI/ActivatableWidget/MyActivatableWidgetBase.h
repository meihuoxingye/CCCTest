#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
// 【极其关键】：generated.h 必须是所有 #include 中的最后一行
#include "MyActivatableWidgetBase.generated.h"

// ==============================================================================
// 广播委托声明：用于解耦 UI 关闭请求
// ==============================================================================
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTacticalMenuCloseRequestedSignature);

/**
 * 可激活控件基类 (Activatable Widget Base)
 * 具备独立生命周期、输入穿透、抗子弹时间与自我状态管理的系统面板祖先
 */
UCLASS(Abstract, Blueprintable)
class CCC_API UMyActivatableWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	// ==============================================================================
	// 事件广播 (Event Broadcasting)
	// ==============================================================================
	UPROPERTY(BlueprintAssignable, Category = "UI|Events")
	FOnTacticalMenuCloseRequestedSignature OnCloseRequested;

	// ==============================================================================
	// 动画与表现系统
	// ==============================================================================
	/** 播放 UI 动画（自动抵消全局极慢子弹时间的影响，保障满帧弹出） */
	UFUNCTION(BlueprintCallable, Category = "UI|Animation")
	void PlayCompensatedAnimation(class UWidgetAnimation* InAnimation, EUMGSequencePlayMode::Type PlayMode = EUMGSequencePlayMode::Forward);

protected:
	// ==============================================================================
	// 核心 UI 动画槽位 (UI Animation Bindings)
	// ==============================================================================

	/** * 出场动画：自动链接蓝图中的同名动画
	 * Transient 标记：防止激进的增量 GC 在慢放时间下误将其回收导致崩溃
	 */
	UPROPERTY(meta = (BindWidgetAnimOptional), Transient)
	TObjectPtr<class UWidgetAnimation> AppearAnim;

	/** 退场动画 */
	UPROPERTY(meta = (BindWidgetAnimOptional), Transient)
	TObjectPtr<class UWidgetAnimation> DisappearAnim;


	// ==============================================================================
	// 底层生命周期
	// ==============================================================================
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual void OnAnimationFinished_Implementation(const UWidgetAnimation* Animation) override;


	// ==============================================================================
	// 输入路由拦截沙箱
	// ==============================================================================
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

public:
	// ==============================================================================
	// 蓝图生命周期钩子
	// ==============================================================================
	UFUNCTION(BlueprintNativeEvent, Category = "UI|Lifecycle")
	void OnWidgetActivated();

	UFUNCTION(BlueprintNativeEvent, Category = "UI|Lifecycle")
	void OnWidgetDeactivated();
};
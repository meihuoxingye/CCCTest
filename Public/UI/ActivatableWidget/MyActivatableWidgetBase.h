// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
// 【必须在最后一行】
#include "MyActivatableWidgetBase.generated.h"

// ==============================================================================
// 事件广播 (Event Broadcasting)
// ==============================================================================
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTacticalMenuCloseRequestedSignature);

// ==============================================================================
// 转换状态机 (Transition State Machine)
// ==============================================================================
UENUM(BlueprintType)
enum class EUIState : uint8
{
	Idle		UMETA(DisplayName = "静止"),
	Opening		UMETA(DisplayName = "展开中"),
	Closing		UMETA(DisplayName = "收起中")
};

// ==============================================================================
// 可激活控件基类 (Activatable Widget Base)
// 纯数学进度驱动版 (Progress-Driven Architecture)
// ==============================================================================
/**
 * 可激活控件基类 (Activatable Widget Base)
 * 3A级战术呼出底层：抗子弹时间、纯数学进度表现驱动、进出场双轨解耦及实时编辑器预览
 */
UCLASS(Abstract, Blueprintable)
class CCC_API UMyActivatableWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	// ==============================================================================
	// 外部委托接口 (External Delegates)
	// ==============================================================================
	UPROPERTY(BlueprintAssignable, Category = "UI|Events")
	FOnTacticalMenuCloseRequestedSignature OnCloseRequested;

protected:
	// ==============================================================================
	// 核心转换系统 (Transition System)
	// ==============================================================================
	/** 核心状态标尺。在 UMG 编辑器细节面板可手动切换状态进行出/退场分流预览。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Transition|State")
	EUIState CurrentState = EUIState::Idle;

	/** 核心进度标尺 (0.0 -> 1.0)。在 UMG 编辑器可拖动滑块实时预览位置和透明度。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Transition|State", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TransitionProgress = 0.0f;

	/** 出场专属耗时（秒）。通常较长，用于展示张力极强的飞入动画 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Transition|Settings", meta = (ClampMin = "0.01"))
	float OpeningDuration = 0.35f;

	/** 退场专属耗时（秒）。通常极短（如 0.15），要求干净利落，不拖泥带水 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Transition|Settings", meta = (ClampMin = "0.01"))
	float ClosingDuration = 0.15f;

	/** 出场缓动指数。推荐 2.0~3.0，飞入时自带丝滑刹车感 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Transition|Settings", meta = (ClampMin = "1.0"))
	float OpeningExp = 2.0f;

	/** 退场缓动指数。推荐 1.0 (纯线性)，让透明度匀速消失，绝不拖泥带水 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Transition|Settings", meta = (ClampMin = "1.0"))
	float ClosingExp = 1.0f;

	// ==============================================================================
	// 核心表现钩子 (Presentation Hooks)
	// ==============================================================================
	/** * 【出场专属钩子】：仅在UI展开时触发。推荐驱动 Y轴位移 和 透明度渐显。
	 * @param Progress  线性进度 (0.0 到 1.0)
	 * @param EasedProgress  平滑后的视觉进度
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Transition")
	void UpdateOpeningEffect(float Progress, float EasedProgress);

	/** * 【退场专属钩子】：仅在UI收起时触发。推荐仅驱动透明度渐隐，利用 Slate 特性维持最后坐标。
	 * @param Progress  线性进度 (0.0 到 1.0)
	 * @param EasedProgress  平滑后的视觉进度
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Transition")
	void UpdateClosingEffect(float Progress, float EasedProgress);

	// ==============================================================================
	// 生命周期与同步 (Lifecycle & Synchronization)
	// ==============================================================================
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** 核心同步函数：编辑器参数修改时热重载表现 */
	virtual void SynchronizeProperties() override;

	// ==============================================================================
	// 响应式输入路由 (Input Routing)
	// ==============================================================================
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

public:
	// ==============================================================================
	// 状态驱动接口 (State Drivers)
	// ==============================================================================
	UFUNCTION(BlueprintNativeEvent, Category = "UI|Lifecycle")
	void OnWidgetActivated();

	UFUNCTION(BlueprintNativeEvent, Category = "UI|Lifecycle")
	void OnWidgetDeactivated();
};
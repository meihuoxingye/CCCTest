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

	// 大管家专线回调：底层无缝漫游死锁结束，新世界准备就绪，命令 UI 开始播放退场动画
	UFUNCTION(BlueprintCallable, Category = "TransitionUI")
	virtual void NotifyEngineReady();

	// 接收大管家查表后下发的最高指令，强行覆写内部电池的物理播放时长
	UFUNCTION(BlueprintCallable, Category = "TransitionUI")
	virtual void SetTransitionDuration(float InIntroDuration);

	// 供带有进度条业务的子类，接收字典时间并进行纯视觉速度平滑测算的通用虚接口
	UFUNCTION(BlueprintCallable, Category = "TransitionUI")
	virtual void SetLoadingTimeConfig(float InMinLoadingTime, float InHoldTime);


	// ==============================================================================
	// 动画渲染驱动源 (Animation Drive Source)
	// ==============================================================================
protected:

	// 3A级工业防呆警告盾牌：在编辑器面板中对美术完全灰色只读，死死钉在参数上方强制警示系统契约
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TransitionUI|Animation", meta = (MultiLine = true))
	FText ArchitectureWarning;

	// 核心装配：将动画模块作为电池嵌入，ShowOnlyInnerProperties 使其内部参数在细节面板中极度优雅地平铺
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TransitionUI|Animation", meta = (ShowOnlyInnerProperties))
	FMyUIAnimationModule AnimModule;

	// 挂起状态互斥锁：用于入场黑屏播完后，UI 陷入绝对安静的死等，直到大管家发送就绪指令
	UPROPERTY(BlueprintReadOnly, Category = "TransitionUI|State")
	bool bIsWaitingForEngine = false;

	// 入场动画渲染钩子：交由蓝图子类实现，接收线性进度与缓动进度，驱动纯视觉的黑幕闭合效果
	UFUNCTION(BlueprintImplementableEvent, Category = "TransitionUI|Animation")
	void UpdateOpeningEffect(float Progress, float EasedProgress);

	// 退场动画渲染钩子：交由蓝图子类实现，接收递减的进度与缓动进度，驱动纯视觉的黑幕擦除效果
	UFUNCTION(BlueprintImplementableEvent, Category = "TransitionUI|Animation")
	void UpdateClosingEffect(float Progress, float EasedProgress);


	// ==============================================================================
	// 控件生命周期 (Widget Lifecycle)
	// ==============================================================================
protected:

	// 覆写底层生命周期：UI 实例化上屏的第一帧，锁定防呆文本并开始执行入场状态机
	virtual void NativeConstruct() override;

	// 所见即所得桥梁：引擎原生数据同步钩子，让美术在编辑器视口拖动参数时能完美预览时间曲线和动画效果
	virtual void SynchronizeProperties() override;

	// 覆写原生帧更新：在此高频驱动电池步进，并在到达临界值时安全触发状态变轨或呼叫大管家
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;


	// 【架构解耦：动画终点纯虚函数钩子】
	// 供不同的业务子类（熄屏、加载屏）注入专属的生命周期事件
	// 入场终点：负责入场动画播完后的业务（如：MyScreenOffWidget 在此发射 Ack）
	virtual void OnOpeningAnimationFinished();

	// 供不同的业务子类（熄屏、加载屏）注入专属的生命周期事件
	// 退场终点：负责退场动画播完后的业务
	virtual void OnClosingAnimationFinished();
};
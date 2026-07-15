// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Transition/MyTransitionWidgetBase.h"
#include "MyLoadingScreenWidget.generated.h"

// 专门负责带材质进度条的转场加载 UI 子类。
// 本类已退化为“纯视觉渲染工具”，生死完全由大管家（GameInstance）的绝对倒计时掌控。
// 内部只负责根据大管家下发的时间参数，通过数学公式计算极其平滑的进度条动态追赶表现。
UCLASS()
class CCC_API UMyLoadingScreenWidget : public UMyTransitionWidgetBase
{
	GENERATED_BODY()

	// ==============================================================================
	// 核心生命周期与组件 (Core Lifecycle & Components)
	// ==============================================================================
protected:
	// UI 物理上屏时触发，负责挂载低频视觉刷新计时器并初始化动态材质
	virtual void NativeConstruct() override;

	// UI 被大管家物理粉碎时触发，负责安全清理内部的视觉刷新计时器
	virtual void NativeDestruct() override;


	// ==============================================================================
	// 材质动态进度条管线 (Material Progress Pipeline)
	// ==============================================================================
public:
	// 纯视觉数据接收接口：获取大管家字典配置的加载总时长与悬停时间
	virtual void SetLoadingTimeConfig(float InMinLoadingTime, float InHoldTime) override;

	// 同步大管家的强制拆除令：大管家拔电时同步知晓并执行退场
	virtual void NotifyEngineReady() override;

protected:
	// 在 UMG 蓝图中必须强绑定且拼写一致的进度条 Image 控件
	UPROPERTY(meta = (BindWidget))
	class UImage* ProgressBarImage;

	// 渲染线程动态材质实例缓存
	UPROPERTY()
	class UMaterialInstanceDynamic* DynamicProgressMID;

	// 驱动纯视觉步长物理更新的内部计时器句柄
	FTimerHandle MaterialProgressTimerHandle;

	// 承载纯视觉步长推进的核心数学状态机
	void UpdateMaterialProgressTick();

	// 物理同步到 GPU 材质里的当前实际视觉百分比数值 (0.0f ~ 1.0f)
	float CurrentVisualPercent = 0.0f;

	// 盲开期（底层引擎尚未 Ready 前）的绝对固定推进速度，默认每秒平稳吃满 15% 进度
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loading Progress")
	float FixedInitialSpeed = 0.15f;

	// 物理时钟模拟更新时间间隔，默认为 0.0333f 秒
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loading Progress")
	float UpdateInterval = 0.0333f;

	// 慢电脑防死机保护大坝，进度条走到此边界后将强制进入极其微弱的衰减蠕动
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loading Progress")
	float PauseThreshold = 0.85f;

	// 【之前被我漏掉的变量 1】：超时兜底极速。如果剩余时间极短，直接用此速度强行冲满
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loading Progress")
	float SprintSpeed = 4.0f;

	// 大管家字典下发的总生存时间备份
	float TargetMinLoadingTime = 0.0f;

	// 大管家字典下发的 100% 满值悬停时间备份
	float TargetHoldTime = 0.0f;

	// UI 运行时长的内部时钟记录器
	float ElapsedTime = 0.0f;

	// 核心数学决策标记：记录速度是否已经基于好电脑就绪进行了重新平滑规划
	bool bVelocityRecomputed = false;

	// 【之前被我漏掉的变量 2】：好电脑就绪时，瞬间算出的平滑抵达终点的完美匀速
	float DynamicCalculatedSpeed = 0.0f;

	// 记录上一帧的绝对物理时间戳，防范主线程 Hitch
	double LastRealTime = 0.0;
};
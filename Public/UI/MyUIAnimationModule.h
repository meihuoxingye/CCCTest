// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Math/UnrealMathUtility.h"
#include "MyUIAnimationModule.generated.h"

// ==============================================================================
// 转换状态机枚举 (Transition State Machine Enum)
// ==============================================================================

// 声明枚举类型，并标记 BlueprintType 使其可以在蓝图系统中作为变量类型被访问
// 使用 uint8 作为底层数据类型以节约内存
UENUM(BlueprintType)
enum class EUIAnimationState : uint8
{
	// 使用 UMETA 宏定义在蓝图编辑器中显示的中文本地化名称
	Idle		UMETA(DisplayName = "静止"),
	Opening		UMETA(DisplayName = "展开中"),
	Closing		UMETA(DisplayName = "收起中")
};

/**
 * UI 动画驱动核心组件 (UI Animation Module Struct)
 * 3A级纯数学进度表现驱动，自带出入场双轨解耦及实时编辑器预览
 */
USTRUCT(BlueprintType)
struct CCC_API FMyUIAnimationModule
{
	GENERATED_BODY()

	// ==============================================================================
	// 核心转换系统配置 (Transition System Settings)
	// ==============================================================================
public:

	// 核心状态标尺。在 UMG 编辑器细节面板可手动切换状态进行出/退场分流预览。
	// 允许在蓝图编辑器面板的任何位置编辑，并且在蓝图图表中可读可写
	// 默认状态初始化为“静止”
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Transition|State")
	EUIAnimationState CurrentState = EUIAnimationState::Idle;

	// 出场专属耗时（秒）。通常较长，用于展示张力极强的飞入动画
	// 设置最小限制为 0.01 秒，从源头防止后续 DeltaTime 除零引发崩溃
	// 默认展开动画耗时 0.35 秒
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Transition|Settings", meta = (ClampMin = "0.01"))
	float OpeningDuration = 0.35f;

	// 退场专属耗时（秒）。通常极短（如 0.15），要求干净利落，不拖泥带水
	// 同样限制最小耗时防崩溃
	// 默认收起动画耗时 0.15 秒
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Transition|Settings", meta = (ClampMin = "0.01"))
	float ClosingDuration = 0.15f;

	// 出场缓动指数。推荐 2.0~3.0，飞入时自带丝滑刹车感
	// 限定缓动指数最小为 1.0 (线性)，用于 FMath::InterpEaseInOut 数学函数
	// 默认开场缓动强度为 2.0
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Transition|Settings", meta = (ClampMin = "1.0"))
	float OpeningExp = 2.0f;

	// 退场缓动指数。推荐 1.0 (纯线性)，让透明度匀速消失，绝不拖泥带水
	// 同样限制最少为纯线性
	// 默认退场缓动强度为 1.0
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Transition|Settings", meta = (ClampMin = "1.0"))
	float ClosingExp = 1.0f;


	// ==============================================================================
	// MVVM 双轨渲染驱动源 (MVVM Dual-Track Drive Source)
	// ==============================================================================
public:

	// 核心时间标尺 (0.0 -> 1.0)，代表当前动画已完成的绝对百分比（现实时间/总时长）。
	// 【底层计算】：由 NativeTick 每帧自动累加或递减。单帧变化步长 = 本帧物理时间差 (DeltaTime) / 动画总耗时 (Duration)。
	// 【架构意义】：将物理秒数强制“归一化”为 0~1 的比例，使后续的缓动曲线公式与蓝图表现彻底脱离具体时长的耦合。
	// 在 UMG 编辑器可拖动滑块实时预览。限制范围在 0.0 到 1.0 之间，避免越界溢出导致后续动画计算错误。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Transition|State", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TransitionProgress = 0.0f;


	// ==============================================================================
	// 纯数学核心时钟引擎 (Pure Math Engine)
	// ==============================================================================
public:

	void StartOpening() { CurrentState = EUIAnimationState::Opening; TransitionProgress = 0.0f; }

	void StartClosing() { CurrentState = EUIAnimationState::Closing; TransitionProgress = 1.0f; }

	// 返回值 true 代表刚好在本帧播放完毕，方便宿主 UI 接收到信号去执行销毁或出栈
	bool Tick(float InDeltaTime)
	{
		// Idle 状态下直接返回，这是最廉价的开销，避免不必要的数学推演
		// 如果处于待机状态，不需要更新动画进度，直接返回以节省性能
		if (CurrentState == EUIAnimationState::Idle) return false;

		// 根据当前是打开还是关闭状态，确定对应的动画总时长
		float CurrentDuration = (CurrentState == EUIAnimationState::Opening) ? OpeningDuration : ClosingDuration;

		// 【优化】：将除法预计算并转为乘法，消除每一帧的除法开销，对 CPU 分支预测更友好
		// 计算本帧进度增加/减少的步长（防止除零错误，最少取 0.001f 应对高帧率显示器）
		const float InvDuration = 1.0f / FMath::Max(0.001f, CurrentDuration);
		const float Step = InDeltaTime * InvDuration;

		float NewProgress = TransitionProgress;

		// 如果正在播放入场动画
		if (CurrentState == EUIAnimationState::Opening)
		{
			// 累加进度，最高不超过 1.0f
			NewProgress = FMath::Min(TransitionProgress + Step, 1.0f);
		}
		// 如果正在播放退场动画
		else if (CurrentState == EUIAnimationState::Closing)
		{
			// 递减进度，最低不低于 0.0f
			NewProgress = FMath::Max(TransitionProgress - Step, 0.0f);
		}

		// 【第一道防线：脏标记防抖拦截 (Dirty Flag Debounce)】
		// 业务背景：当 UI 播放出场/退场动画时，本类会在短短 0.3 秒内，亲自每帧向这里塞入递增/递减的进度。
		// 灾难规避：此处必须执行严格的内存值比对。一旦拦截到因超高帧率导致 DeltaTime 过小而产生的无效重复数字，立刻掐断更新。
		// （宿主 UI 也可借此截断 MVVM 无效广播），死守渲染管线的 CPU 消耗底线。
		if (TransitionProgress == NewProgress) return false;

		// 数据物理着陆 (记录最新的 0~1 线性进度)
		TransitionProgress = NewProgress;

		// 再次判断状态，用于应用动画效果及判定动画是否结束
		if (CurrentState == EUIAnimationState::Opening)
		{
			// 如果进度达到或超过 1.0，说明入场动画播放完毕
			if (TransitionProgress >= 1.0f)
			{
				CurrentState = EUIAnimationState::Idle; // 切换为待机状态
				return true;
			}
		}
		else if (CurrentState == EUIAnimationState::Closing)
		{
			// 状态机收尾：当进度彻底归零（退场动画播完）
			if (TransitionProgress <= 0.0f)
			{
				// 将状态重置为待机
				CurrentState = EUIAnimationState::Idle;
				return true;
			}
		}

		return false;
	}

	// ==============================================================================
	// 【双轨渲染之第二轨：数学引擎与蓝图交接 (Math Engine & Blueprint Handover)】
	// ==============================================================================
	// 性能与表现的完美平衡：C++ 承担极其昂贵的非线性数学插值计算 (InterpEaseInOut)，
	// 算出带有丝滑惯性的 EasedProgress 后，将其直接拍给蓝图的 UpdateOpeningEffect 钩子。
	// 让蓝图只做它最擅长的事——把现成的算好的数字连到 Transform 节点上。
	float GetEasedProgress() const
	{
		// 如果当前 UI 正在打开或者处于待机状态
		if (CurrentState == EUIAnimationState::Opening || CurrentState == EUIAnimationState::Idle)
		{
			// 计算经过缓动曲线（EaseInOut）处理后的当前进度值
			return FMath::InterpEaseInOut(0.0f, 1.0f, TransitionProgress, OpeningExp);
		}
		// 如果当前 UI 正在关闭
		else if (CurrentState == EUIAnimationState::Closing)
		{
			// 【执行第二轨：退场非线性渲染】
			// 计算经过缓动曲线处理后的关闭动画进度值
			return FMath::InterpEaseInOut(0.0f, 1.0f, TransitionProgress, ClosingExp);
		}

		return 0.0f;
	}
};
// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Transition/MyTransitionWidgetBase.h"
#include "Game/MyGameInstance.h"
#include "Kismet/GameplayStatics.h"


// ==============================================================================
// 外部大管家契约接口 (External Master Contracts)
// ==============================================================================
#pragma region

void UMyTransitionWidgetBase::NotifyEngineReady()
{
	// 状态锁拦截：只有在处于“黑屏挂起死等”状态时，才接受大管家的退场指令
	if (bIsWaitingForEngine)
	{
		// 状态机切换：解除等待锁
		bIsWaitingForEngine = false;

		// 呼叫内部电池：开始跑退场动画，进度将平滑从 1.0 往 0.0 衰减
		AnimModule.StartClosing();
	}
}

void UMyTransitionWidgetBase::SetTransitionDuration(float InIntroDuration)
{
	// 安全拦截防除零：确保传入的时间参数合法
	if (InIntroDuration > 0.0f)
	{
		// 强行覆写内部电池的入场时间，彻底剥夺 UI 自身的控制权，实现大管家绝对的数据驱动
		AnimModule.OpeningDuration = InIntroDuration;
	}
}

void UMyTransitionWidgetBase::SetLoadingTimeConfig(float InMinLoadingTime, float InHoldTime)
{
	// 架构契约：基类属于通用的过渡黑幕，没有复杂的进度条业务
	// 此处故意留空，专门等待带有进度条的子类 (MyLoadingScreenWidget) 进行多态重写覆盖
}

#pragma endregion


// ==============================================================================
// 控件生命周期 (Widget Lifecycle)
// ==============================================================================
#pragma region

void UMyTransitionWidgetBase::NativeConstruct()
{
	// 必须调用父类原生构造，完成 UMG 基础渲染树的搭建
	Super::NativeConstruct();

	// 拦截器：防止 UMG 编辑器预览模式意外执行运行时的动画核心逻辑导致崩溃
	if (IsDesignTime()) return;

	// 防呆文本物理着陆：在 C++ 层硬编码锁死警告内容，防止策划或美术在蓝图中意外修改
	ArchitectureWarning = NSLOCTEXT("Architecture", "TransitionWarning",
		"【系统时序契约】：\n"
		"1. 此处的 Opening/Closing Duration 在本类中仅供【编辑器视口拖动预览】！\n"
		"2. 游戏运行时，此处的具体秒数将被完全废弃，系统将强行注入关卡触发器(Trigger)中配置的物理时间！\n"
		"3. 美术仅需在此类中调节缓动指数(Exp)和视觉效果，绝对不要和策划硬刚运行时时间！");

	// 初始化状态机，明确当前处于初生入场阶段，而非挂起等待阶段
	bIsWaitingForEngine = false;

	// 时序起跑线：UI 实例化上屏的第一帧，直接命令电池开始跑入场动画，绝不拖泥带水
	AnimModule.StartOpening();
}

void UMyTransitionWidgetBase::SynchronizeProperties()
{
	// 调用父类原生属性同步，响应编辑器面板的参数修改
	Super::SynchronizeProperties();

	// 强制刷写：保证编辑器模式下，美术一编译或者一改参数，警告文本就能在细节面板里被强制覆盖为官方契约
	ArchitectureWarning = NSLOCTEXT("Architecture", "TransitionWarning",
		"【系统时序契约】：\n"
		"1. 此处的 Opening/Closing Duration 在本类中仅供【编辑器视口拖动预览】！\n"
		"2. 游戏运行时，此处的具体秒数将被完全废弃，系统将强行注入关卡触发器(Trigger)中配置的物理时间！\n"
		"3. 美术仅需在此类中调节缓动指数(Exp)和视觉效果，绝对不要和策划硬刚运行时时间！");

	// 编辑器实时预览魔法：完美复用运行时的电池逻辑，将美术拖动的虚假进度映射到视觉效果上
	if (AnimModule.CurrentState == EUIAnimationState::Opening || AnimModule.CurrentState == EUIAnimationState::Idle)
	{
		// 驱动蓝图表现层进行入场（黑屏）效果的预览更新
		UpdateOpeningEffect(AnimModule.TransitionProgress, AnimModule.GetEasedProgress());
	}
	else if (AnimModule.CurrentState == EUIAnimationState::Closing)
	{
		// 驱动蓝图表现层进行退场（亮屏）效果的预览更新
		UpdateClosingEffect(AnimModule.TransitionProgress, AnimModule.GetEasedProgress());
	}
}

void UMyTransitionWidgetBase::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	// 调用父类原生 Tick 驱动底层 UI 刷新
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 性能与状态拦截：如果是编辑器模式，或者处于黑屏挂起等待引擎的状态，立刻切断 Tick 计算，极致节省 CPU 开销
	if (IsDesignTime() || bIsWaitingForEngine) return;

	// 驱动内部动画电池步进，并获取本帧是否刚好跑完（到达 0.0 或 1.0 的终点）
	bool bJustFinished = AnimModule.Tick(InDeltaTime);

	// 状态机路由：如果正在入场，或者刚好在这一帧完成了入场（进度顶满 1.0）
	if (AnimModule.CurrentState == EUIAnimationState::Opening || (bJustFinished && AnimModule.TransitionProgress >= 1.0f))
	{
		// 驱动蓝图视觉表现层进行纯粹的视觉差值更新
		UpdateOpeningEffect(AnimModule.TransitionProgress, AnimModule.GetEasedProgress());

		// 当电池报告“刚刚跑完入场”时
		if (bJustFinished)
		{
			// 状态机切换：进入最高戒备的“挂起”状态，死等大管家发送的引擎就绪信号
			bIsWaitingForEngine = true;
		}
	}
	// 状态机路由：如果正在退场，或者刚好在这一帧完成了退场（进度衰减至 0.0）
	else if (AnimModule.CurrentState == EUIAnimationState::Closing || (bJustFinished && AnimModule.TransitionProgress <= 0.0f))
	{
		// 驱动蓝图视觉表现层擦除黑幕
		UpdateClosingEffect(AnimModule.TransitionProgress, AnimModule.GetEasedProgress());

		// 当电池报告“刚刚跑完退场”时，意味着屏幕已经完全亮起，旧世界记忆彻底清空
		if (bJustFinished)
		{
			// 安全获取当前世界的转场大管家实例
			if (UMyGameInstance* GI = Cast<UMyGameInstance>(UGameplayStatics::GetGameInstance(this)))
			{
				// 反向呼叫大管家：我已彻底退场，请从内存层面物理销毁我并释放引用链！完美闭环！
				GI->FinalizeLoadingScreenRemoval();
			}
		}
	}
}

#pragma endregion
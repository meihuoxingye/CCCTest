// Fill out your copyright notice in the Description page of Project Settings.

#include "Component/UI/MyUIHandlerComponent.h"
#include "UI/MyMainHUDWidget.h"
#include "Game/MyGameModeBase.h"
#include "Character/TopCharacter.h"
#include "UI/ActivatableWidget/MyActivatableWidgetBase.h"
#include "EnhancedInputSubsystems.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
// 引入控制器
#include "Character/TopPlayerController.h"

UMyUIHandlerComponent::UMyUIHandlerComponent()
{
	// 组件在逻辑上没有每帧更新的需求
	PrimaryComponentTick.bCanEverTick = false;
}

// ==============================================================================
// UI 统筹系统 (UI Management System)
// ==============================================================================
#pragma region

void UMyUIHandlerComponent::BeginPlay()
{
	Super::BeginPlay();

	// 【进阶优化】：在 BeginPlay 阶段即刻获取并锁定控制器指针
	// 避免后续在每一帧或每次按键高频交互时进行昂贵的 Cast 转换
	CachedPC = Cast<ATopPlayerController>(GetOwner());
	if (!CachedPC) return;

	// 【架构核心】：主动监听 GameMode 的名册更新频道，实现真正的事件驱动
	if (AMyGameModeBase* GM = Cast<AMyGameModeBase>(GetWorld()->GetAuthGameMode()))
	{
		// 无论何时，只要有人调用了 Broadcast，底层的事件系统会自动唤醒这里的 UpdateHUD 进行刷新
		GM->OnRosterChanged.AddUniqueDynamic(this, &UMyUIHandlerComponent::UpdateHUD);
	}

	// HUD 控件的初始化与首帧构建
	if (MainHUDClass)
	{
		// 创造主 UI 组件，拥有者为当前玩家控制器
		MainHUDInstance = CreateWidget<UMyMainHUDWidget>(CachedPC, MainHUDClass);

		if (MainHUDInstance)
		{
			// 将主 UI 组件添加到视口
			MainHUDInstance->AddToViewport();
			// 更新 UI 界面
			UpdateHUD();
		}
	}
}

void UMyUIHandlerComponent::UpdateHUD()
{
	// 如果 UI 面板还未就绪，或者不在游戏世界里，直接返回
	if (!MainHUDInstance || !GetWorld()) return;

	// O(1) 极速调取：向当前 GameMode 索要已过滤好、最干净的存活友军名单
	if (AMyGameModeBase* GM = Cast<AMyGameModeBase>(GetWorld()->GetAuthGameMode()))
	{
		// 【进阶优化】：直接使用预先缓存的 CachedPC 提取 Pawn，消灭 Cast<APlayerController>(GetOwner())
		ATopCharacter* MyPawn = CachedPC ? Cast<ATopCharacter>(CachedPC->GetPawn()) : nullptr;
		// 传递名单，并将当前玩家控制器所附身的 Pawn 转化为 ATopCharacter 作为当前活跃单位传入
		MainHUDInstance->UpdateSquadList(GM->FriendlyRoster, MyPawn);
	}
}

#pragma endregion

// ==============================================================================
// 战术指令与总线转发 (Tactical Commands & Bus Forwarding)
// ==============================================================================
#pragma region


void UMyUIHandlerComponent::ToggleTacticalWidget()
{
	ToggleTacticalWidget(!bIsTacticalUIOpen);
}

void UMyUIHandlerComponent::ToggleTacticalWidget(bool bShouldOpen)
{
	// 使用 O(1) 缓存的控制器指针进行拦截
	if (!CachedPC) return;

	// 【进阶优化：状态原子性锁】：如果目标状态与当前状态完全一致，则直接掐断逻辑！
	// 这从根本上杜绝了网络延迟或极短时间内重复触发导致的 UI 重复压栈和状态撕裂
	if (bIsTacticalUIOpen == bShouldOpen) return;

	// 懒加载模式：如果战术 UI 实例还未创建，且蓝图里配置了具体的类模板，则开始创建
	if (!TacticalWidgetInstance && TacticalWidgetClass)
	{
		// 在当前控制器的内存名下创建这个战术 UI 蓝图的实例
		TacticalWidgetInstance = CreateWidget<UMyActivatableWidgetBase>(CachedPC, TacticalWidgetClass);

		// 确保 UI 实例创建成功
		if (TacticalWidgetInstance)
		{
			// 添加到屏幕视口，ZOrder 设置为 100 保证它盖在游戏画面和其他普通 UI 之上
			// ZOrder（Z 轴排序/层级），数字越大，这个 UI 所在的层就越靠前，会遮挡住底下的 UI
			TacticalWidgetInstance->AddToViewport(100);
			// 初始创建时强制设为隐藏（Collapsed），防止在还没播放动画时出现一帧的画面闪烁
			TacticalWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);

			// 绑定到当前组件特制的转发函数，用于通知控制器！
			TacticalWidgetInstance->OnCloseRequested.AddUniqueDynamic(this, &UMyUIHandlerComponent::HandleWidgetCloseRequested);
		}
	}

	// 安全校验：如果 UI 实例依然为空（比如粗心没配置类模板），直接退出防止引发野指针崩溃
	if (!TacticalWidgetInstance) return;

	// 获取增强输入系统的本地玩家子系统，用于后续插拔输入映射上下文 (IMC)
	auto* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(CachedPC->GetLocalPlayer());

	// 【安全应用明确的目标状态】
	bIsTacticalUIOpen = bShouldOpen;

	// 如果判定为：准备打开战术面板
	if (bIsTacticalUIOpen)
	{
		// 必须在此处显式恢复可视状态
		TacticalWidgetInstance->SetVisibility(ESlateVisibility::Visible);
		// 呼叫 UI 实例执行它自己的“被激活”逻辑（比如重置进度、播放展开动画、主动入栈防穿透）
		TacticalWidgetInstance->OnWidgetActivated();

		// 为增强输入系统挂载战术面板专用的 IMC，优先级设为 10（高于默认的 0）
		// IMC（输入映射上下文）是可以像穿衣服一样一层一层“穿上”和“脱下”的
		// 若此 IMC 的优先级更高，则会覆盖掉底层的开火、移动等操作
		if (Subsystem && TacticalIMC) Subsystem->AddMappingContext(TacticalIMC, 10);
	}

	// 如果判定为：准备关闭战术面板
	else
	{
		// 呼叫 UI 实例执行它自己的“反激活”逻辑（比如切换状态机、播放收起动画并在动画结束时自动出栈）
		TacticalWidgetInstance->OnWidgetDeactivated();

		// 从输入系统中剥夺战术面板专属的 IMC，把按键映射还给正常的 3D 游戏操作
		// RemoveMappingContext 卸载 IMC
		if (Subsystem && TacticalIMC) Subsystem->RemoveMappingContext(TacticalIMC);

		// 强制将引擎底层的“输入焦点”从 UI 身上剥离，并交还给 3D 游戏视口。这彻底解决了 UI 关闭后第一下鼠标点击无效的问题
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
	}
}

void UMyUIHandlerComponent::HandleWidgetCloseRequested()
{
	// 【进阶优化】：通过 BeginPlay 中预先缓存好的 CachedPC 进行 O(1) 极速调用，彻底消除 Cast 类型转换开销
	// 【逻辑还原】：当 UI 内部（如点击背景、按取消键）发来关闭请求时
	// 向上汇报给控制器大管家，让控制器去同时关闭 UI + 退出子弹时间！
	if (CachedPC)
	{
		CachedPC->ToggleTacticalMode();
	}
}

#pragma endregion

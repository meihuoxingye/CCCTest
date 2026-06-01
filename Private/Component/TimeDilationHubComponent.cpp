// Fill out your copyright notice in the Description page of Project Settings.

#include "Component/TimeDilationHubComponent.h"
// 全局时空控制
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
// UI材质参数控制
#include "Materials/MaterialParameterCollection.h" 
#include "Kismet/KismetMaterialLibrary.h"

UTimeDilationHubComponent::UTimeDilationHubComponent()
{
	// 必须开启 Tick，这正是本组件存在的核心意义
	PrimaryComponentTick.bCanEverTick = true;
}

void UTimeDilationHubComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UTimeDilationHubComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UWorld* World = GetWorld();
	if (!World) return;

	// 安全检查：检查我们在蓝图里配置的 MPC（全局材质参数集合）资产是否有效
	if (GlobalUIMPC.Get())
	{
		UKismetMaterialLibrary::SetScalarParameterValue(
			World,
			GlobalUIMPC.Get(),
			FName("GlobalGameTime"),
			World->GetTimeSeconds()
		);
	}


	// ===================== 【切人模式：平滑子弹时间】 =====================
	// 获取当前游戏世界的真实时间流速
	// 比如正常流速是 1.0，如果进入子弹时间，它现在可能是 0.3
	float CurrentDilation = UGameplayStatics::GetGlobalTimeDilation(World);

	// 容差判断。判断“当前的流速”和“我们想要达到的流速”是不是已经差不多相等了
	// 0.005f 是容差（误差范围）。如果不加这个判断，Tick 函数每帧都会做无意义的计算，浪费 CPU 性能
	if (!FMath::IsNearlyEqual(CurrentDilation, TargetTimeDilation, 0.005f))
	{
		// 反向还原真实世界的 DeltaTime
		// 坑点说明：虚幻引擎的 DeltaTime（两帧之间经过的时间）是受全局时间流速影响的
		// 如果当前流速变成了 0.1，那么原本 0.016 秒的 DeltaTime 也会缩水成 0.0016 秒
		// 如果你用缩水的 DeltaTime 去做插值，减速过程就会变得越来越慢，永远也减不到 0.05
		// 解决办法：用缩水的 DeltaTime 除以当前的流速，硬生生把它“还原”成现实世界中玩家真实度过的时间（UnscaledDelta）
		// FMath::Max(0.001f, CurrentDilation) 是为了防止除以 0 导致游戏直接崩溃
		// 不用 0.016（固定的 1/60 秒），因为不同电脑的帧率不同
		float UnscaledDelta = DeltaTime / FMath::Max(0.001f, CurrentDilation);

		// 执行平滑插值计算
		// FMath::FInterpTo 是虚幻自带的非线性缓动插值函数（先快后慢，极其平滑）
		// 它拿着我们上面算出的“现实真实时间差(UnscaledDelta)”，以 15.f 的速度，把当前流速向目标流速推进
		// 15.f 这个数值越大，过渡越快；越小，过渡越拖沓。15.f 大约能在 0.1~0.2 秒内极其顺滑地完成过渡，完美防止音频撕裂
		float NewDilation = FMath::FInterpTo(CurrentDilation, TargetTimeDilation, UnscaledDelta, 15.f);

		// 强行对齐兜底
		// 坑点说明：FInterpTo 这种非线性插值，永远只能“无限逼近”目标值，但理论上永远达不到（比如 0.99, 0.999, 0.9999...）
		// 所以我们手动加个判断：只要计算出来的新流速和目标流速的差距小于 0.01 了，别挣扎了，直接把数值强行等于目标值
		// 这样可以彻底结束插值过程，让外层的 IsNearlyEqual 判定生效，停止 Tick 计算
		if (FMath::Abs(NewDilation - TargetTimeDilation) < 0.01f)
		{
			NewDilation = TargetTimeDilation;
		}

		// 把最终计算好的、完美平滑的新流速，塞回给虚幻引擎的世界中，立刻生效
		UGameplayStatics::SetGlobalTimeDilation(World, NewDilation);
	}
}


// ==============================================================================
// 战术指令与时间流速系统 (Tactical Time & Switch System)
// ==============================================================================

// 【新增核心修复】：唯一的流速写入入口，严格遵循状态优先级
void UTimeDilationHubComponent::UpdateTargetTimeDilation()
{
	if (bIsSwitchModeActive)
	{
		// 优先级 1：只要切人模式开着，必定是极慢（0.05）
		TargetTimeDilation = 0.05f;
	}
	else if (bIsBulletTime)
	{
		// 优先级 2：切人没开，但按了中键，进入普通子弹时间（0.1）
		TargetTimeDilation = 0.1f;
	}
	else
	{
		// 优先级 3：都关了，恢复正常
		TargetTimeDilation = 1.0f;
	}
}

// 切换是否处于“切人子弹时间”状态
void UTimeDilationHubComponent::SetSwitchMode(bool bEnable)
{
	// 状态拦截：如果外部传来的状态和当前一样，直接掐断，防止无意义的仲裁运算
	if (bIsSwitchModeActive == bEnable) return;
	bIsSwitchModeActive = bEnable;

	// 呼叫仲裁器
	UpdateTargetTimeDilation();
}

// 绑定给 Tab 键的输入动作，反转 bIsSwitchModeActive 状态
void UTimeDilationHubComponent::ToggleSwitchMode()
{
	SetSwitchMode(!bIsSwitchModeActive);
}

// 触发原版的普通子弹时间技能
void UTimeDilationHubComponent::ToggleBulletTime()
{
	bIsBulletTime = !bIsBulletTime;

	// 呼叫仲裁器
	UpdateTargetTimeDilation();
}
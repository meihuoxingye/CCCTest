// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimeDilationHubComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class CCC_API UTimeDilationHubComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTimeDilationHubComponent();


	// ==============================================================================
	// 核心生命周期 (Core Lifecycle)
	// ==============================================================================
public:
	// 这是整个时间组件存在的绝对核心：执行平滑插值
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;


	// ==============================================================================
	// 战术指令与时间流速系统 (Tactical Time & Switch System)
	// ==============================================================================
public:
	// 状态锁：当前是否处于“切人子弹时间”
	bool bIsSwitchModeActive = false;

	// 若是否处于“切人子弹时间”状态与传入状态不一致，则切换状态
	// 换人成功或左键点地面取消时，传入 false 则可强行把状态设为 false 恢复正常时间
	// Tab 键与中键的回调函数中调用
	void SetSwitchMode(bool bEnable);

	// 【新增防线：物理急停开关】
	// 用于转场、死亡等极端情况，强制撕毁一切子弹时间状态，物理回归 1.0 流速！
	UFUNCTION(BlueprintCallable, Category = "Time")
	void ForceResetTime();

	// 绑定给 Tab 键的输入动作，反转 bIsSwitchModeActive 状态
	void ToggleSwitchMode();

	// 触发原版的普通子弹时间技能
	void ToggleBulletTime();

protected:
	// 在蓝图中配置创建的材质参数集合资产（MPC）
	// 【架构优化】：现在由专门的时间组件来负责向 GPU 推送时间参数
	UPROPERTY(EditDefaultsOnly, Category = "Optimization")
	TObjectPtr<class UMaterialParameterCollection> GlobalUIMPC;

private:
	bool bIsBulletTime = false;

	// 目标时间流速：1.0 (正常) 或 0.05 (极慢)
	// 避坑：不用引擎瞬间变速，防止音频爆音和画面卡顿，由 Tick 读取此值进行平滑插值
	float TargetTimeDilation = 1.0f;

	// 【新增核心修复】：中央状态仲裁器。
	// 防止多个减速技能互相踩踏，严格按照优先级来决定最终流速
	void UpdateTargetTimeDilation();
};
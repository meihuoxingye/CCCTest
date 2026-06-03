// Fill out your copyright notice in the Description page of Project Settings.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "TopPlayerController.generated.h"

UCLASS()
class CCC_API ATopPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ATopPlayerController();

	// ==============================================================================
	// 核心生命周期与组件 (Core Lifecycle & Components)
	// ==============================================================================
protected:
	virtual void BeginPlay() override;

	// 【核心挂件】：时空枢纽组件，全权负责平滑子弹时间与渲染同步
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class UTimeDilationHubComponent> TimeDilationHub;


	// ==============================================================================
	// UI 统筹系统 (UI Management System)
	// ==============================================================================
public:
	// 主动调用以刷新当前界面布局
	UFUNCTION(BlueprintCallable, Category = "UI")
	void UpdateHUD();

protected:
	// 暴露给蓝图，用于在编辑器中指定主 HUD 面板的具体蓝图类
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<class UMyMainHUDWidget> MainHUDClass;

	// 缓存创建出来的主 HUD 实例，用于后续在 C++ 中随时呼叫 UI 刷新
	UPROPERTY()
	TObjectPtr<class UMyMainHUDWidget> MainHUDInstance;

	// ---------------- 【新增开始】 ----------------
	/** 战术呼出控件类 (强约束：必须继承自可激活基类) */
	UPROPERTY(EditDefaultsOnly, Category = "UI|Tactical")
	TSubclassOf<class UMyActivatableWidgetBase> TacticalWidgetClass;

private:
	/** 瞬时缓存的战术 UI 实例，使用 Transient 防存档污染 */
	UPROPERTY(Transient)
	TObjectPtr<class UMyActivatableWidgetBase> TacticalWidgetInstance;
	// ---------------- 【新增结束】 ----------------


	// ==============================================================================
	// 战术指令与总线转发 (Tactical Commands & Bus Forwarding)
	// ==============================================================================
public:
	// 换人系统总线的最终执行者：接收 UI 传来的“目标角色”并执行灵魂交接
	void SwitchToSpecificCharacter(class ATopCharacter* TargetCharacter);

	// 只读查询现在是否处于切人模式（子弹时间）状态
	bool IsSwitchModeActive() const;
	// 设置是否进入切人模式
	// 角色攻击回调函数已调用，必返回退出切人模式的指令
	// 
	void SetSwitchMode(bool bEnable);


	// ==============================================================================
	// 灵魂附身与输入绑定 (Possession & Input Setup)
	// ==============================================================================
protected:
	// 键位绑定回调函数
	virtual void SetupInputComponent() override;

	// 缓存角色指针，避免每帧都寻找
	UPROPERTY()
	TObjectPtr<class ATopCharacter> CachedMyCharacter;

	// 当控制器开始控制一个 Pawn 时触发，缓存找到的角色，只找一次
	virtual void OnPossess(APawn* InPawn) override;
	// 当控制器不再控制时将指针清空
	virtual void OnUnPossess() override;

private:
	// 增强输入资产配置，蓝图中配置
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<class UInputMappingContext> TopContext;

	// ---------------- 【新增开始】 ----------------
	/** 战术模式专用拦截上下文 (挂载优先级 10 的黑洞 IMC) */
	UPROPERTY(EditAnywhere, Category = "Input|Tactical")
	TObjectPtr<class UInputMappingContext> TacticalIMC;
	// ---------------- 【新增结束】 ----------------

	// 绑定给中键的输入动作，绑定的回调函数在时间膨胀组件中
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<class UInputAction> BulletTimeAction;
	// 绑定给 Tab 键的输入动作，绑定的回调函数在时间膨胀组件中
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> SwitchModeAction;

	// ---------------- 【新增开始】 ----------------
	// 战术模式总开关：处理中键按下时的子弹时间与 UI 呼出
	// 只需这一个纯净的宏，让虚幻底层能找到它即可
	UFUNCTION()
	void ToggleTacticalWidget();
	// ---------------- 【新增结束】 ----------------
};
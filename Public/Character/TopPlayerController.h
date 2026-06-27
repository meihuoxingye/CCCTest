// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
// 【新增】：包含 UI 契约接口
#include "Interaction/MyPlayerUIInterface.h"
#include "TopPlayerController.generated.h"

class UTimeDilationHubComponent;
class UMyUIHandlerComponent;
class ATopCharacter;
class UInputMappingContext;
class UInputAction;

UCLASS()
class CCC_API ATopPlayerController : public APlayerController, public IMyPlayerUIInterface // 【新增】：继承接口
{
	GENERATED_BODY()

public:
	ATopPlayerController();

	// ==============================================================================
	// 核心生命周期与组件 (Core Lifecycle & Components)
	// ==============================================================================
protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override; // <--- 新增这行

	// 【核心挂件】：时空枢纽组件，全权负责平滑子弹时间与渲染同步
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTimeDilationHubComponent> TimeDilationHub;

	// 【核心挂件】：UI 处理组件，全权负责 UI 的实例化、入栈出栈以及面板渲染刷新
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UMyUIHandlerComponent> UIHandlerComp;

public:
	// 开放给外界索要 UI 处理组件的快捷接口
	FORCEINLINE UMyUIHandlerComponent* GetUIHandler() const { return UIHandlerComp; }


	// ==============================================================================
	// 战术指令与总线转发 (Tactical Commands & Bus Forwarding)
	// ==============================================================================
public:
	// 【全局解耦接口】：主交互输入（鼠标左键/确认键）的前置统筹处理器
	// 核心职责：在角色执行底层 3D 行为（如武器开火）前，综合判定本次点击是否应被高层系统（UI 物理拦截、或特殊游戏状态）接管与消耗。
	// 架构约束：
	// 1. 【统一收口】：作为防穿透门神，未来所有直接由鼠标左键触发的 3D 世界交互，均必须优先调用此接口进行审查。
	// 2. 【重构预警】：当前采用线性分支统筹 UI 与切人逻辑；若未来互斥的输入状态激增（如引入建造模式、技能指示器），必须将此函数内部重构为“状态机”或“责任链”。
	// 返回值：若输入已被高层逻辑消耗处理，返回 true 以熔断底层的 3D 执行逻辑。
	bool ProcessGlobalClick();

	// 换人系统总线的最终执行者：接收 UI 传来的“目标角色”并执行灵魂交接
	// 核心职责：处理角色附身逻辑，并安全清理残留的 UI 状态与输入焦点
	void SwitchToSpecificCharacter(ATopCharacter* TargetCharacter);

	// 只读查询现在是否处于切人模式（子弹时间）状态
	bool IsSwitchModeActive() const;
	// 设置是否进入切人模式
	// 角色攻击回调函数已调用，必返回退出切人模式的指令
	void SetSwitchMode(bool bEnable);

	// 【修改】：处理中键按下时的战术模式（子弹时间与 UI）宏观统筹切换
	UFUNCTION()
	void ToggleTacticalMode();


	// ==============================================================================
	// 玩家 UI 交互契约实现 (Player UI Interface Implementation)
	// ==============================================================================
public:
	// 契约 1：处理呼出/关闭存档面板请求
	virtual void ToggleSaveMenu_Implementation() override;

	// 契约 2：处理 UI 发来的换人请求
	// 发起方：UMyCharacterStatusWidget::OnAvatarClicked()：当玩家点击角色头像时
	// 接收方：ATopPlayerController::RequestCharacterSwitch_Implementation()：转接到具体处理角色附身逻辑的函数
	virtual void RequestCharacterSwitch_Implementation(class ABaseCharacter* TargetCharacter) override;


	// ==============================================================================
	// 灵魂附身与输入绑定 (Possession & Input Setup)
	// ==============================================================================
protected:
	// 键位绑定回调函数
	virtual void SetupInputComponent() override;

	// 缓存角色指针，避免每帧都寻找
	UPROPERTY()
	TObjectPtr<ATopCharacter> CachedMyCharacter;

	// 当控制器开始控制一个 Pawn 时触发，缓存找到的角色，只找一次
	virtual void OnPossess(APawn* InPawn) override;
	// 当控制器不再控制时将指针清空
	virtual void OnUnPossess() override;

private:
	// 增强输入资产配置，蓝图中配置
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputMappingContext> TopContext;

	// 绑定给中键的输入动作
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> BulletTimeAction;
	// 绑定给 Tab 键的输入动作，绑定的回调函数在时间膨胀组件中
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> SwitchModeAction;
};
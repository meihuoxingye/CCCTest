// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MyUIHandlerComponent.generated.h"

class UMyMainHUDWidget;
class UMyActivatableWidgetBase;
class UInputMappingContext;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class CCC_API UMyUIHandlerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMyUIHandlerComponent();

	// ==============================================================================
	// UI 统筹系统 (UI Management System)
	// ==============================================================================
public:
	// 主动调用以刷新当前界面布局
	// 现已被绑定至 GameMode 的更新总线上自动调用
	UFUNCTION(BlueprintCallable, Category = "UI")
	void UpdateHUD();

protected:
	virtual void BeginPlay() override;

	// 暴露给蓝图，用于在编辑器中指定主 HUD 面板的具体蓝图类
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UMyMainHUDWidget> MainHUDClass;

	// 缓存创建出来的主 HUD 实例，用于后续在 C++ 中随时呼叫 UI 刷新
	UPROPERTY()
	TObjectPtr<UMyMainHUDWidget> MainHUDInstance;

	// 【进阶优化】：缓存组件拥有者的玩家控制器，彻底消除高频触发时的 Cast 类型转换开销
	UPROPERTY()
	TObjectPtr<class ATopPlayerController> CachedPC;

	/** 战术呼出控件类 (强约束：必须继承自可激活基类) */
	UPROPERTY(EditDefaultsOnly, Category = "UI|Tactical")
	TSubclassOf<UMyActivatableWidgetBase> TacticalWidgetClass;

	/** 战术模式专用拦截上下文 (挂载优先级 10 的黑洞 IMC) */
	UPROPERTY(EditAnywhere, Category = "Input|Tactical")
	TObjectPtr<UInputMappingContext> TacticalIMC;

private:
	/** 瞬时缓存的战术 UI 实例，使用 Transient 防存档污染 */
	UPROPERTY(Transient)
	TObjectPtr<UMyActivatableWidgetBase> TacticalWidgetInstance;

	// 战术面板是否开启
	// 极其稳定的状态锁，用来代替之前依赖 IMC 的脆弱判断：系统是否开启，全凭自己内部的变量说了算
	bool bIsTacticalUIOpen = false;

	// ==============================================================================
	// 战术指令与总线转发 (Tactical Commands & Bus Forwarding)
	// ==============================================================================
public:
	// 切换战术 UI 面板的状态 ：向下调用带参数的原子性切换版本，进行 UI 状态翻转
	// 对外保留的无参翻转接口，保持与原控制器兼容
	// 1.增强输入系统的回调函数绑定时无法传入 bool 参数，因此需要此无参接口
	// 2.外界不用获取当前 bIsTacticalUIOpen 状态也能通过这个无参接口切换 UI 状态，降低耦合
	void ToggleTacticalWidget();

	// 【进阶优化】：设置战术 UI 面板的状态 (带明确目标状态参数的原子性切换接口)
	void ToggleTacticalWidget(bool bShouldOpen);

	// 委托回调函数：专门接收 UI 内部发出的关闭请求，向上汇报给控制器
	UFUNCTION()
	void HandleWidgetCloseRequested();
};
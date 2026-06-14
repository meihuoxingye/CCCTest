// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/ActivatableWidget/MyActivatableWidgetBase.h"
#include "MySaveMenuWidget.generated.h"

class UButton;
class UTextBlock;
class UListView;
class UMySaveSlotWidget;

// ==============================================================================
// 存档菜单面板 (Save Menu Widget)
// ==============================================================================
UCLASS()
class CCC_API UMySaveMenuWidget : public UMyActivatableWidgetBase
{
	GENERATED_BODY()

	// ==============================================================================
	// 核心生命周期与初始化 (Core Lifecycle & Initialization)
	// ==============================================================================
public:
	// 构造函数
	UMySaveMenuWidget();

protected:
	// UI 蓝图预构建阶段：负责在底层 Slate 控件成型前，处理属性配置（如剥夺按钮焦点）
	virtual void NativePreConstruct() override;

	// UI 的生命周期函数：创建时调用
	virtual void NativeConstruct() override;

	// UI 的生命周期函数：销毁时调用（极其重要，用于解绑委托防止内存泄漏）
	virtual void NativeDestruct() override;

	// ==============================================================================
	// 动态列表生成 (Dynamic List Generation)
	// ==============================================================================
protected:

	// 强制要求蓝图必须有一个叫 List_SaveSlots 的 ListView 控件，否则 C++ 直接报错拒绝编译
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UListView> List_SaveSlots;

	// 暴露给蓝图，让美术指定要在上面的 ListView 控件里生成哪个槽位蓝图 (比如 WBP_SaveSlot)
	UPROPERTY(EditDefaultsOnly, Category = "SaveMenu|Classes")
	TSubclassOf<UMySaveSlotWidget> SlotWidgetClass;

	// 执行 C++ 极速列表构建
	// 为什么必须加 UFUNCTION()？因为这个函数在 cpp 中被绑定到了动态多播委托 (AddDynamic) 上。
	// 虚幻的反射系统要求，凡是绑定到动态委托的函数，必须戴上 UFUNCTION 标签，否则引擎运行时找不到这个函数！
	UFUNCTION()
	void BuildSaveSlotList();

	// ==============================================================================
	// 底部新建档位逻辑与全局状态监听 (New Save & Global State)
	// ==============================================================================
protected:

	// 新建存档按钮
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_CreateNewSave;

	// 存档状态提示文本（如“正在保存”、“保存成功”）
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_SaveStatus;

	// 点击“新建存档”按钮的回调函数，必须加 UFUNCTION 以便绑定到 OnClicked 委托
	UFUNCTION()
	void OnCreateNewSaveClicked();

	// 接收大管家（SaveSubsystem）存盘完成后的广播回调
	UFUNCTION()
	void HandleSaveFinished(bool bSuccess);

	// 预留给 UI 蓝图实现的硬盘数据同步完成时的动画接口。C++ 只负责逻辑，播放流光特效或动画的脏活累活交由蓝图去连节点；
	// 父级的 Opening/Closing：控制的是面板的“生存状态”；子级的 SaveSuccess：控制的是具体的“业务状态”。
	UFUNCTION(BlueprintImplementableEvent, Category = "SaveMenu|Animation")
	void BP_PlaySaveSuccessAnimation();
};
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/ActivatableWidget/MyActivatableWidgetBase.h"
#include "MySaveMenuWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;
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
	// 绝对排版与分页系统 (Absolute Layout & Pagination)
	// ==============================================================================
protected:

	// 强制要求蓝图必须有一个叫 Box_SaveSlots 的垂直框，否则 C++ 直接报错拒绝编译
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> Box_SaveSlots;

	// 页码显示 (如 "1 / 3")
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_PageInfo;

	// 翻页控制按钮
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_PrevPage;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_NextPage;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_AddPage;

	// 暴露给蓝图，让美术指定要生成的卡片类 (如 WBP_SaveSlot)
	UPROPERTY(EditDefaultsOnly, Category = "SaveMenu|Classes")
	TSubclassOf<UMySaveSlotWidget> SlotWidgetClass;

private:

	// 内存状态锁与物理写死限制
	int32 CurrentPage = 1;
	const int32 SlotsPerPage = 5;
	const int32 MaxAllowedPages = 50;

	// 【新增】：核心 UI 对象池，在内存中死死捏住这 5 个卡片的物理地址
	UPROPERTY()
	TArray<TObjectPtr<UMySaveSlotWidget>> SlotPool;

	// 仅在 Construct 时调用一次，蓄满对象池并构建焦点铁壁
	void InitializeSlotPool();

	// 执行 C++ 极速 O(1) 提取数据并刷新池子里的卡片长相
	UFUNCTION()
	void BuildSaveSlotList();

	// 刷新分页按钮状态
	void RefreshPaginationUI(int32 TotalPages);

	UFUNCTION()
	void OnPrevPageClicked();

	UFUNCTION()
	void OnNextPageClicked();

	UFUNCTION()
	void OnAddPageClicked();

	// ==============================================================================
	// 全局状态监听 (Global State)
	// ==============================================================================
protected:

	// 接收大管家（SaveSubsystem）存盘完成后的广播回调
	UFUNCTION()
	void HandleSaveFinished(bool bSuccess);

	// 预留给 UI 蓝图实现的硬盘数据同步完成时的动画接口。C++ 只负责逻辑，播放流光特效或动画的脏活累活交由蓝图去连节点；
	// 父级的 Opening/Closing：控制的是面板的“生存状态”；子级的 SaveSuccess：控制的是具体的“业务状态”。
	UFUNCTION(BlueprintImplementableEvent, Category = "SaveMenu|Animation")
	void BP_PlaySaveSuccessAnimation();
};
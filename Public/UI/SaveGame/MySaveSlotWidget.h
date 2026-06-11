// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SaveGame/MySaveGame.h" 
#include "MySaveSlotWidget.generated.h"

class UButton;

// ==============================================================================
// 存档槽位条目基类 (Save Slot Entry Widget Base)
// ==============================================================================

UCLASS(Abstract, Blueprintable)
class CCC_API UMySaveSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ==============================================================================
	// 核心数据分发接口 (Core Data Distribution Interface)
	// ==============================================================================

	// 接收主菜单传来的数据，并初始化自身状态
	UFUNCTION(BlueprintCallable, Category = "SaveSystem|Slot")
	void InitializeSlotData(const FSaveSlotMetaData& SlotMetaData);

protected:
	virtual void NativeConstruct() override;

	// ==============================================================================
	// 控件绑定与交互逻辑 (Widget Bindings & Interactions)
	// ==============================================================================

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Save;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Load;

	// 【新增 1】：绑定蓝图里的删除按钮 (确保你在蓝图里命名为 Btn_DeleteSlot)
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_DeleteSlot;

	UFUNCTION()
	void OnSaveButtonClicked();

	UFUNCTION()
	void OnLoadButtonClicked();

	// 【新增 2】：删除按钮的回调函数
	UFUNCTION()
	void OnDeleteSlotClicked();

	// 蓝图必须实现的事件：用于将 C++ 数据渲染到文本框上
	UFUNCTION(BlueprintImplementableEvent, Category = "SaveSystem|Slot")
	void BP_OnSlotDataInitialized(const FSaveSlotMetaData& SlotMetaData);

private:
	// 缓存当前槽位物理文件名
	FString CachedSlotName;
};
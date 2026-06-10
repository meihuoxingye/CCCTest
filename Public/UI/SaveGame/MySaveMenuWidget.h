// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/ActivatableWidget/MyActivatableWidgetBase.h"
#include "MySaveMenuWidget.generated.h"

class UButton;
class UTextBlock;
class UScrollBox;
class UMySaveSlotWidget;

// ==============================================================================
// 存档菜单面板 (Save Menu Widget)
// ==============================================================================

UCLASS()
class CCC_API UMySaveMenuWidget : public UMyActivatableWidgetBase
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// ==============================================================================
	// 动态列表生成 (Dynamic List Generation)
	// ==============================================================================

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> Scroll_SlotList;

	// 暴露给蓝图，让美术指定要生成哪个槽位蓝图 (比如 WBP_SaveSlot)
	UPROPERTY(EditDefaultsOnly, Category = "SaveMenu|Classes")
	TSubclassOf<UMySaveSlotWidget> SlotWidgetClass;

	// 执行 C++ 极速列表构建
	void BuildSaveSlotList();

	// ==============================================================================
	// 底部新建档位逻辑与全局状态监听 (New Save & Global State)
	// ==============================================================================

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_CreateNewSave;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_SaveStatus;

	UFUNCTION()
	void OnCreateNewSaveClicked();

	UFUNCTION()
	void HandleSaveFinished(bool bSuccess);

	UFUNCTION(BlueprintImplementableEvent, Category = "SaveMenu|Animation")
	void BP_PlaySaveSuccessAnimation();
};
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/IUserObjectListEntry.h" // 必须引入列表接口
#include "SaveGame/MySaveGame.h" 
#include "MySaveSlotWidget.generated.h"

class UButton;

// ==============================================================================
// 【重构】：多重继承 IUserObjectListEntry
// ==============================================================================
UCLASS(Abstract, Blueprintable)
class CCC_API UMySaveSlotWidget : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	// 【重构核心】：ListView 虚拟化回掉。当列表给当前控件分配新数据时触发！
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

	// ==============================================================================
	// 控件绑定与交互逻辑
	// ==============================================================================
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Save;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Load;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_DeleteSlot;

	UFUNCTION()
	void OnSaveButtonClicked();

	UFUNCTION()
	void OnLoadButtonClicked();

	UFUNCTION()
	void OnDeleteSlotClicked();

	UFUNCTION(BlueprintImplementableEvent, Category = "SaveSystem|Slot")
	void BP_OnSlotDataInitialized(const FSaveSlotMetaData& SlotMetaData);

private:
	FString CachedSlotName;
};
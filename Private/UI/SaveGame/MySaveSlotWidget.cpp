// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/SaveGame/MySaveSlotWidget.h"
#include "Components/Button.h"
#include "Engine/GameInstance.h"
#include "SaveGame/Subsystem/MySaveSubsystem.h"
#include "UI/SaveGame/MySaveDataObj.h"

#pragma region

void UMySaveSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_Save)
	{
		Btn_Save->OnClicked.AddDynamic(this, &UMySaveSlotWidget::OnSaveButtonClicked);
	}
	if (Btn_Load)
	{
		Btn_Load->OnClicked.AddDynamic(this, &UMySaveSlotWidget::OnLoadButtonClicked);
	}
	if (Btn_DeleteSlot)
	{
		Btn_DeleteSlot->OnClicked.AddDynamic(this, &UMySaveSlotWidget::OnDeleteSlotClicked);
	}
}

// ListView 循环复用该控件时，会把轻量级的 DataObj 塞进来
void UMySaveSlotWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	if (UMySaveDataObj* DataObj = Cast<UMySaveDataObj>(ListItemObject))
	{
		// 更新真名缓存，供按钮使用
		CachedSlotName = DataObj->MetaData.SlotName;

		// 呼叫蓝图更改 UI 的文本和图像显示
		BP_OnSlotDataInitialized(DataObj->MetaData);
	}
}

void UMySaveSlotWidget::OnSaveButtonClicked()
{
	if (CachedSlotName.IsEmpty()) return;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMySaveSubsystem* SaveSub = GI->GetSubsystem<UMySaveSubsystem>())
		{
			SaveSub->PerformAsyncSave(CachedSlotName);
		}
	}
}

void UMySaveSlotWidget::OnLoadButtonClicked()
{
	if (CachedSlotName.IsEmpty()) return;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMySaveSubsystem* SaveSub = GI->GetSubsystem<UMySaveSubsystem>())
		{
			SaveSub->LoadGameFromSlot(CachedSlotName);
		}
	}
}

void UMySaveSlotWidget::OnDeleteSlotClicked()
{
	if (CachedSlotName.IsEmpty()) return;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMySaveSubsystem* SaveSub = GI->GetSubsystem<UMySaveSubsystem>())
		{
			SaveSub->DeleteSaveSlot(CachedSlotName);
		}
	}
}

#pragma endregion
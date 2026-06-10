// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/SaveGame/MySaveSlotWidget.h"
#include "Components/Button.h"
#include "Engine/GameInstance.h"
#include "SaveGame/Subsystem/MySaveSubsystem.h"

// ==============================================================================
// 存档槽位条目基类 (Save Slot Entry Widget Base)
// ==============================================================================
#pragma region

void UMySaveSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 纯 C++ 绑定按钮，蓝图里连线都不用拉了
	if (Btn_Save)
	{
		Btn_Save->OnClicked.AddDynamic(this, &UMySaveSlotWidget::OnSaveButtonClicked);
	}
	if (Btn_Load)
	{
		Btn_Load->OnClicked.AddDynamic(this, &UMySaveSlotWidget::OnLoadButtonClicked);
	}
}

void UMySaveSlotWidget::InitializeSlotData(const FSaveSlotMetaData& SlotMetaData)
{
	CachedSlotName = SlotMetaData.SlotName;

	// 通知蓝图更新文本等纯视觉表现
	BP_OnSlotDataInitialized(SlotMetaData);
}

void UMySaveSlotWidget::OnSaveButtonClicked()
{
	if (CachedSlotName.IsEmpty()) return;

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMySaveSubsystem* SaveSub = GI->GetSubsystem<UMySaveSubsystem>())
		{
			// 触发异步覆盖写入
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
			// 触发时空回溯 (物理位置与系统状态瞬移)
			SaveSub->LoadGameFromSlot(CachedSlotName);
		}
	}
}

#pragma endregion
// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/SaveGame/MySaveMenuWidget.h"
#include "UI/SaveGame/MySaveSlotWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/ListView.h" // 替换了 ScrollBox.h
#include "UI/SaveGame/MySaveDataObj.h" // 引入数据壳子
#include "SaveGame/Subsystem/MySaveSubsystem.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"
#include "Engine/World.h"

// ==============================================================================
// 存档菜单面板 (Save Menu Widget)
// ==============================================================================
#pragma region

void UMySaveMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_CreateNewSave)
	{
		Btn_CreateNewSave->OnClicked.AddDynamic(this, &UMySaveMenuWidget::OnCreateNewSaveClicked);
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMySaveSubsystem* SaveSub = GI->GetSubsystem<UMySaveSubsystem>())
		{
			SaveSub->OnSaveFinished.AddDynamic(this, &UMySaveMenuWidget::HandleSaveFinished);

			// 【修复 3】：只要大管家喊“删档了”，这里全自动调用你写好的极速重建函数！
			SaveSub->OnSaveRegistryChanged.AddDynamic(this, &UMySaveMenuWidget::BuildSaveSlotList);
		}
	}

	BuildSaveSlotList();
}

void UMySaveMenuWidget::NativeDestruct()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMySaveSubsystem* SaveSub = GI->GetSubsystem<UMySaveSubsystem>())
		{
			SaveSub->OnSaveFinished.RemoveDynamic(this, &UMySaveMenuWidget::HandleSaveFinished);
			// 【安全解绑】
			SaveSub->OnSaveRegistryChanged.RemoveDynamic(this, &UMySaveMenuWidget::BuildSaveSlotList);
		}
	}
	Super::NativeDestruct();
}

void UMySaveMenuWidget::BuildSaveSlotList()
{
	if (!List_SaveSlots) return;

	List_SaveSlots->ClearListItems();

	// 【附带修复】：每次刷新列表，清空状态文本
	if (Text_SaveStatus)
	{
		Text_SaveStatus->SetText(FText::GetEmpty());
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMySaveSubsystem* SaveSub = GI->GetSubsystem<UMySaveSubsystem>())
		{
			TArray<FSaveSlotMetaData> MetaList = SaveSub->GetSaveSlotList();

			// C++ 极速实例化并推入滚动框
			for (const FSaveSlotMetaData& Meta : MetaList)
			{
				// 将数据装入 UObject 壳子并喂给 ListView
				UMySaveDataObj* DataObj = NewObject<UMySaveDataObj>();
				DataObj->MetaData = Meta;
				List_SaveSlots->AddItem(DataObj);
			}
		}
	}
}

void UMySaveMenuWidget::OnCreateNewSaveClicked()
{
	if (!Btn_CreateNewSave) return;

	Btn_CreateNewSave->SetIsEnabled(false);

	if (Text_SaveStatus)
	{
		Text_SaveStatus->SetText(FText::FromString(TEXT("正在开辟新扇区...")));
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMySaveSubsystem* SaveSub = GI->GetSubsystem<UMySaveSubsystem>())
		{
			// 利用当前时间戳生成绝对唯一的新槽位名
			FString NewSlotName = TEXT("Save_") + FDateTime::Now().ToString();
			SaveSub->PerformAsyncSave(NewSlotName);
		}
	}
}

void UMySaveMenuWidget::HandleSaveFinished(bool bSuccess)
{
	if (bSuccess)
	{
		if (Text_SaveStatus)
		{
			Text_SaveStatus->SetText(FText::FromString(TEXT("数据同步完成。")));
		}

		// ==========================================
		// 【修复 1】：立刻重建列表，让新建立的存档瞬间刷新出来！
		// ==========================================
		BuildSaveSlotList();

		// ==========================================
		// 【修复 2】：立刻把变灰的按钮恢复。因为 UI 是常驻内存的，不恢复的话下次打开依然是灰的！
		// ==========================================
		if (Btn_CreateNewSave)
		{
			Btn_CreateNewSave->SetIsEnabled(true);
		}

		BP_PlaySaveSuccessAnimation();
	}
	else
	{
		if (Text_SaveStatus)
		{
			Text_SaveStatus->SetText(FText::FromString(TEXT("写入失败，请重试。")));
		}
		if (Btn_CreateNewSave)
		{
			Btn_CreateNewSave->SetIsEnabled(true);
		}
	}
}

#pragma endregion
// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/SaveGame/MySaveMenuWidget.h"
#include "UI/SaveGame/MySaveSlotWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/ScrollBox.h"
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

	// 1. 绑定新建档位按钮
	if (Btn_CreateNewSave)
	{
		Btn_CreateNewSave->OnClicked.AddDynamic(this, &UMySaveMenuWidget::OnCreateNewSaveClicked);
	}

	// 2. 监听全局总线：无论哪个子槽位触发了存档，主面板都能听到并更新 UI 状态
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMySaveSubsystem* SaveSub = GI->GetSubsystem<UMySaveSubsystem>())
		{
			SaveSub->OnSaveFinished.AddDynamic(this, &UMySaveMenuWidget::HandleSaveFinished);
		}
	}

	// 3. 构建动态列表
	BuildSaveSlotList();
}

void UMySaveMenuWidget::NativeDestruct()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMySaveSubsystem* SaveSub = GI->GetSubsystem<UMySaveSubsystem>())
		{
			SaveSub->OnSaveFinished.RemoveDynamic(this, &UMySaveMenuWidget::HandleSaveFinished);
		}
	}
	Super::NativeDestruct();
}

void UMySaveMenuWidget::BuildSaveSlotList()
{
	if (!Scroll_SlotList || !SlotWidgetClass) return;

	Scroll_SlotList->ClearChildren();

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMySaveSubsystem* SaveSub = GI->GetSubsystem<UMySaveSubsystem>())
		{
			TArray<FSaveSlotMetaData> MetaList = SaveSub->GetSaveSlotList();

			// C++ 极速实例化并推入滚动框
			for (const FSaveSlotMetaData& Meta : MetaList)
			{
				if (UMySaveSlotWidget* NewSlotWidget = CreateWidget<UMySaveSlotWidget>(this, SlotWidgetClass))
				{
					NewSlotWidget->InitializeSlotData(Meta);
					Scroll_SlotList->AddChild(NewSlotWidget);
				}
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

		BP_PlaySaveSuccessAnimation();

		FTimerHandle TimerHandle;
		TWeakObjectPtr<UMySaveMenuWidget> WeakThis(this);

		GetWorld()->GetTimerManager().SetTimer(TimerHandle, [WeakThis]()
			{
				if (WeakThis.IsValid())
				{
					WeakThis->OnWidgetDeactivated();
				}
			}, 0.6f, false);
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
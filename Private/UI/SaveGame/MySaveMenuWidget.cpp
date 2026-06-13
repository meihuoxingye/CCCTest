// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/SaveGame/MySaveMenuWidget.h"
#include "UI/SaveGame/MySaveSlotWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/ListView.h" 
#include "UI/SaveGame/MySaveDataObj.h" 
#include "SaveGame/Subsystem/MySaveSubsystem.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"
#include "Engine/World.h"

// ==============================================================================
// 核心生命周期与初始化 (Core Lifecycle & Initialization)
// ==============================================================================
#pragma region

void UMySaveMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 1. 绑定新建按钮的点击事件
	if (Btn_CreateNewSave)
	{
		Btn_CreateNewSave->OnClicked.AddDynamic(this, &UMySaveMenuWidget::OnCreateNewSaveClicked);
	}

	// 2. 找到全局的存档大管家，戴上耳机监听它的广播
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMySaveSubsystem* SaveSub = GI->GetSubsystem<UMySaveSubsystem>())
		{
			// 监听频道A：存盘是否成功（成功了才去播放动画、恢复按钮可用状态）
			SaveSub->OnSaveFinished.AddDynamic(this, &UMySaveMenuWidget::HandleSaveFinished);

			// 监听频道B：注册表是否发生变化（有人删档或新建档）
			// 只要大管家喊“删档了”，这里全自动调用极速重建函数，实现数据的绝对同步响应！
			SaveSub->OnSaveRegistryChanged.AddDynamic(this, &UMySaveMenuWidget::BuildSaveSlotList);
		}
	}

	// 3. 面板刚打开时，主动去拉取一次当前已有的存档列表进行显示
	BuildSaveSlotList();
}

void UMySaveMenuWidget::NativeDestruct()
{
	// 内存安全生命线：UI 被销毁时，必须摘下耳机，取消一切订阅！
	// 防止 UI 被垃圾回收后，大管家还在向这块死亡的内存发送广播，导致游戏直接崩溃（野指针）
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMySaveSubsystem* SaveSub = GI->GetSubsystem<UMySaveSubsystem>())
		{
			// 安全解绑监听事件
			SaveSub->OnSaveFinished.RemoveDynamic(this, &UMySaveMenuWidget::HandleSaveFinished);
			SaveSub->OnSaveRegistryChanged.RemoveDynamic(this, &UMySaveMenuWidget::BuildSaveSlotList);
		}
	}

	Super::NativeDestruct();
}

#pragma endregion

// ==============================================================================
// 动态列表生成 (Dynamic List Generation)
// ==============================================================================
#pragma region

void UMySaveMenuWidget::BuildSaveSlotList()
{
	if (!List_SaveSlots) return;

	// 先把旧列表彻底清空，准备重新生成
	List_SaveSlots->ClearListItems();

	// 每次刷新列表，清空底部的状态文本（防止残留上次的“正在保存”字样）
	if (Text_SaveStatus)
	{
		Text_SaveStatus->SetText(FText::GetEmpty());
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMySaveSubsystem* SaveSub = GI->GetSubsystem<UMySaveSubsystem>())
		{
			// 从大管家的内存镜像中，瞬间 O(1) 拉取所有的存档摘要数据
			TArray<FSaveSlotMetaData> MetaList = SaveSub->GetSaveSlotList();

			// C++ 极速实例化并推入滚动框
			for (const FSaveSlotMetaData& Meta : MetaList)
			{
				// UListView 不接受普通结构体，只接受 UObject 对象作为数据源
				// 所以用一个极轻量级的 UMySaveDataObj 数据壳子，把纯数据 Meta 包装进去，再喂给 ListView
				UMySaveDataObj* DataObj = NewObject<UMySaveDataObj>();
				DataObj->MetaData = Meta;
				List_SaveSlots->AddItem(DataObj);
			}
		}
	}
}

#pragma endregion

// ==============================================================================
// 底部新建档位逻辑与全局状态监听 (New Save & Global State)
// ==============================================================================
#pragma region

void UMySaveMenuWidget::OnCreateNewSaveClicked()
{
	if (!Btn_CreateNewSave) return;

	// 防玩家手贱设计：玩家点完新建存档后，立刻把按钮变灰（禁用）
	// 防止玩家在硬盘写入期间疯狂点击，生成几百个同样的存档导致崩溃
	Btn_CreateNewSave->SetIsEnabled(false);

	if (Text_SaveStatus)
	{
		Text_SaveStatus->SetText(FText::FromString(TEXT("正在开辟新扇区...")));
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMySaveSubsystem* SaveSub = GI->GetSubsystem<UMySaveSubsystem>())
		{
			// 利用当前系统绝对时间戳生成档位名（例如：Save_2026.06.12-11.55.00）
			// 确保就算玩家疯狂新建，每个档位在硬盘上的名字也绝对唯一，绝不会相互覆盖
			FString NewSlotName = TEXT("Save_") + FDateTime::Now().ToString();

			// 把名字丢给大管家，开启后台异步写盘
			SaveSub->PerformAsyncSave(NewSlotName);
		}
	}
}

void UMySaveMenuWidget::HandleSaveFinished(bool bSuccess)
{
	// 收到大管家传回来的异步写盘结果
	if (bSuccess)
	{
		if (Text_SaveStatus)
		{
			Text_SaveStatus->SetText(FText::FromString(TEXT("数据同步完成。")));
		}

		// 立刻重建列表，让新建立的存档瞬间刷新出现在 ListView 的最顶端
		BuildSaveSlotList();

		// 立刻把变灰的按钮恢复可用
		// 因为 UI 使用了 ActivatableWidget 架构，面板是常驻隐藏的，不恢复下次打开按钮还是灰的
		if (Btn_CreateNewSave)
		{
			Btn_CreateNewSave->SetIsEnabled(true);
		}

		// 触发蓝图动画，给玩家一个酷炫的视觉反馈
		BP_PlaySaveSuccessAnimation();
	}
	else
	{
		// 极低概率触发（硬盘满、权限不足）的错误托底
		if (Text_SaveStatus)
		{
			Text_SaveStatus->SetText(FText::FromString(TEXT("写入失败，请重试。")));
		}

		// 即使失败了，也要把按钮还给玩家，让他们可以再试一次
		if (Btn_CreateNewSave)
		{
			Btn_CreateNewSave->SetIsEnabled(true);
		}
	}
}

#pragma endregion
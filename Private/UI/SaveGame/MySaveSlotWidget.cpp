// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/SaveGame/MySaveSlotWidget.h"
#include "UI/SaveGame/MySaveDataObj.h"
#include "Components/Button.h"
#include "Engine/GameInstance.h"
#include "SaveGame/Subsystem/MySaveSubsystem.h"

#include "UI/CommonButton/MyCommonButtonBase.h" 


// ==============================================================================
// 生命周期与核心虚函数 (Lifecycle & Core Virtual Functions)
// ==============================================================================
#pragma region

void UMySaveSlotWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
}

// 实现 UI 创建时的初始化逻辑
void UMySaveSlotWidget::NativeConstruct()
{
	// 必须调用父类 UUserWidget 的构造逻辑，完成底层 Slate 控件树的搭建
	Super::NativeConstruct();

	// 防御性编程：检查保存按钮是否已被蓝图正确绑定，防止蓝图损坏导致空指针崩溃
	if (Btn_Save)
	{
		// 将保存按钮的点击事件动态绑定到本类的响应函数上
		Btn_Save->OnClicked().AddUObject(this, &UMySaveSlotWidget::OnSaveButtonClicked);
	}

	// 检查读取按钮是否有效
	if (Btn_Load)
	{
		// 绑定读取按钮的点击事件
		Btn_Load->OnClicked().AddUObject(this, &UMySaveSlotWidget::OnLoadButtonClicked);
	}

	// 检查删除按钮是否有效
	if (Btn_DeleteSlot)
	{
		// 绑定删除按钮的点击事件
		Btn_DeleteSlot->OnClicked().AddUObject(this, &UMySaveSlotWidget::OnDeleteSlotClicked);
	}
}

void UMySaveSlotWidget::NativeDestruct()
{
	// 【对策 1 落地】：在生命周期结束时，强制断开所有原生 C++ 引用，规避垃圾回收期（GC）空指针崩溃
	if (Btn_Save)
	{
		Btn_Save->OnClicked().RemoveAll(this);
	}

	if (Btn_Load)
	{
		Btn_Load->OnClicked().RemoveAll(this);
	}

	if (Btn_DeleteSlot)
	{
		Btn_DeleteSlot->OnClicked().RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UMySaveSlotWidget::SetSlotViewModel(UMySaveDataObj* InViewModel)
{
	if (SlotViewModel != InViewModel)
	{
		SlotViewModel = InViewModel;
		// 【核心修复 3】：使用 UHT 生成的静态 FieldId，通知 UMG 面板 ViewModel 底座发生变更
		BroadcastFieldValueChanged(UMySaveSlotWidget::FFieldNotificationClassDescriptor::SlotViewModel);
	}

	// 兼容原有的表现层红节点，确保你原本蓝图里连的那些 SetText 节点一字不改依然有效
	if (SlotViewModel)
	{
		BP_OnSlotDataInitialized(SlotViewModel->GetMetaData(), !SlotViewModel->GetIsEmptySlot());
	}
	else
	{
		BP_OnSlotDataInitialized(FSaveSlotMetaData(), false);
	}
}

#pragma endregion

// ==============================================================================
// 控件绑定与交互逻辑 (Widget Binding & Interaction Logic)
// ==============================================================================
#pragma region

// 实现保存按钮的点击逻辑
void UMySaveSlotWidget::OnSaveButtonClicked()
{
	// 极度严谨的防御锁：直接向 ViewModel 索要缓存的主键，绝不向底层发脏指令
	if (!SlotViewModel || SlotViewModel->GetSlotName().IsEmpty()) return;

	// 尝试获取当前世界的全局游戏实例
	if (UGameInstance* GI = GetGameInstance())
	{
		// 从游戏实例中拉取负责统筹存档的 MySaveSubsystem
		if (UMySaveSubsystem* SaveSub = GI->GetSubsystem<UMySaveSubsystem>())
		{
			// 拿着缓存的身份证号（档位名），命令子系统执行多线程异步物理覆盖存档（或新建存档）
			SaveSub->PerformAsyncSave(SlotViewModel->GetSlotName());
		}
	}
}

// 实现读取按钮的点击逻辑
void UMySaveSlotWidget::OnLoadButtonClicked()
{
	// 查验当前卡片是否绑定了有效的档位名主键
	if (!SlotViewModel || SlotViewModel->GetSlotName().IsEmpty()) return;

	// 获取游戏实例
	if (UGameInstance* GI = GetGameInstance())
	{
		// 获取存档总管子系统
		if (UMySaveSubsystem* SaveSub = GI->GetSubsystem<UMySaveSubsystem>())
		{
			// 拿着档位名，命令子系统执行同步读盘，强行覆盖当前游戏世界的状态
			SaveSub->LoadGameFromSlot(SlotViewModel->GetSlotName());
		}
	}
}

// 实现删除按钮的点击逻辑
void UMySaveSlotWidget::OnDeleteSlotClicked()
{
	// 查验有效性，防止误删或传空字符导致底层崩溃
	if (!SlotViewModel || SlotViewModel->GetSlotName().IsEmpty()) return;

	// 获取游戏实例
	if (UGameInstance* GI = GetGameInstance())
	{
		// 获取存档总管子系统
		if (UMySaveSubsystem* SaveSub = GI->GetSubsystem<UMySaveSubsystem>())
		{
			// 命令子系统从硬盘彻底抹除此档位，子系统删完后会自动广播通知上级菜单刷新此卡片
			SaveSub->DeleteSaveSlot(SlotViewModel->GetSlotName());
		}
	}
}

#pragma endregion
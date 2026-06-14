// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/SaveGame/MySaveSlotWidget.h"
#include "Components/Button.h"
#include "Engine/GameInstance.h"
#include "SaveGame/Subsystem/MySaveSubsystem.h"


// ==============================================================================
// 生命周期与核心虚函数 (Lifecycle & Core Virtual Functions)
// ==============================================================================
#pragma region

void UMySaveSlotWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// 无法通过 C++ 设置按钮能否聚焦，要到蓝图设置不可聚焦
	/*
	// 防御性编程：检查保存按钮是否已被蓝图正确绑定，防止蓝图损坏导致空指针崩溃
	if (Btn_Save)
	{
		// 不允许按钮本身获取焦点
		UMyUITools::SetButtonFocusable(Btn_Save, false);
	}

	// 检查读取按钮是否有效
	if (Btn_Load)
	{
		// 不允许按钮本身获取焦点
		UMyUITools::SetButtonFocusable(Btn_Load, false);
	}

	// 检查删除按钮是否有效
	if (Btn_DeleteSlot)
	{
		// 不允许按钮本身获取焦点
		UMyUITools::SetButtonFocusable(Btn_DeleteSlot, false);
	}
	*/
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
		Btn_Save->OnClicked.AddDynamic(this, &UMySaveSlotWidget::OnSaveButtonClicked);
	}

	// 检查读取按钮是否有效
	if (Btn_Load)
	{
		// 绑定读取按钮的点击事件
		Btn_Load->OnClicked.AddDynamic(this, &UMySaveSlotWidget::OnLoadButtonClicked);
	}

	// 检查删除按钮是否有效
	if (Btn_DeleteSlot)
	{
		// 绑定删除按钮的点击事件
		Btn_DeleteSlot->OnClicked.AddDynamic(this, &UMySaveSlotWidget::OnDeleteSlotClicked);
	}
}

void UMySaveSlotWidget::InitSlotData(const FString& InSlotName, const FSaveSlotMetaData* MetaData)
{
	// 更新真名缓存，将物理存档名（如"SaveSlot_xxx"）缓存到本卡片的私有变量中，供按钮交互时当作主键使用
	CachedSlotName = InSlotName;

	// 如果 O(1) 查表找到了数据，给蓝图传入真和拆解好的 MetaData，驱动表现层去改文字和截图
	if (MetaData)
	{
		BP_OnSlotDataInitialized(*MetaData, true);
	}
	// 如果是个空档位，给蓝图传入假
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
	// 极度严谨的防御锁：如果当前卡片是个空壳（没分配档位名），直接掐断逻辑，绝不向底层发脏指令
	if (CachedSlotName.IsEmpty()) return;

	// 尝试获取当前世界的全局游戏实例
	if (UGameInstance* GI = GetGameInstance())
	{
		// 从游戏实例中拉取负责统筹存档的 MySaveSubsystem
		if (UMySaveSubsystem* SaveSub = GI->GetSubsystem<UMySaveSubsystem>())
		{
			// 拿着缓存的身份证号（档位名），命令子系统执行多线程异步物理覆盖存档（或新建存档）
			SaveSub->PerformAsyncSave(CachedSlotName);
		}
	}
}

// 实现读取按钮的点击逻辑
void UMySaveSlotWidget::OnLoadButtonClicked()
{
	// 查验当前卡片是否绑定了有效的档位名主键
	if (CachedSlotName.IsEmpty()) return;

	// 获取游戏实例
	if (UGameInstance* GI = GetGameInstance())
	{
		// 获取存档总管子系统
		if (UMySaveSubsystem* SaveSub = GI->GetSubsystem<UMySaveSubsystem>())
		{
			// 拿着档位名，命令子系统执行同步读盘，强行覆盖当前游戏世界的状态
			SaveSub->LoadGameFromSlot(CachedSlotName);
		}
	}
}

// 实现删除按钮的点击逻辑
void UMySaveSlotWidget::OnDeleteSlotClicked()
{
	// 查验有效性，防止误删或传空字符导致底层崩溃
	if (CachedSlotName.IsEmpty()) return;

	// 获取游戏实例
	if (UGameInstance* GI = GetGameInstance())
	{
		// 获取存档总管子系统
		if (UMySaveSubsystem* SaveSub = GI->GetSubsystem<UMySaveSubsystem>())
		{
			// 命令子系统从硬盘彻底抹除此档位，子系统删完后会自动广播通知上级菜单刷新此卡片
			SaveSub->DeleteSaveSlot(CachedSlotName);
		}
	}
}

#pragma endregion
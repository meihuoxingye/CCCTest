// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/SaveGame/MySaveSlotWidget.h"
#include "Components/Button.h"
#include "Engine/GameInstance.h"
#include "SaveGame/Subsystem/MySaveSubsystem.h"
#include "UI/SaveGame/MySaveDataObj.h"

// ==============================================================================
// 生命周期与核心虚函数 (Lifecycle & Core Virtual Functions)
// ==============================================================================
#pragma region

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

// ListView 循环复用该控件时，会把轻量级的 DataObj 塞进来
// 实现接口回调：当卡片被复用并注入新数据时触发
void UMySaveSlotWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	// 将引擎传来的泛型 UObject 安全向下转型为我们自定义的数据壳子
	if (UMySaveDataObj* DataObj = Cast<UMySaveDataObj>(ListItemObject))
	{
		// 更新真名缓存，供按钮使用
		// 剥离出壳子中的元数据，将物理存档名（如"Save_xxx"）缓存到本卡片的私有变量中，供按钮交互时当作主键使用
		CachedSlotName = DataObj->MetaData.SlotName;

		// 呼叫蓝图更改 UI 的文本和图像显示
		// C++ 只做数据拆解，将拆好的 MetaData 丢给蓝图事件，驱动表现层去改文字和截图
		BP_OnSlotDataInitialized(DataObj->MetaData);
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
			// 拿着缓存的身份证号（档位名），命令子系统执行多线程异步物理覆盖存档
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
			// 命令子系统从硬盘彻底抹除此档位，子系统删完后会自动广播通知上级菜单移除此卡片
			SaveSub->DeleteSaveSlot(CachedSlotName);
		}
	}
}

#pragma endregion
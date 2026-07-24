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

void UMySaveSlotWidget::NativeOnInitialized()
{
	// 必须加上这行，MVVM 是作为 UUserWidget 的一个扩展组件存在的。
	// 当 UI 被创建时，引擎必须通过执行父类 UUserWidget::NativeOnInitialized() 来初始化这些扩展组件，并在底层建立起所有的 MVVM 监听网络。
	Super::NativeOnInitialized();

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

void UMySaveSlotWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
}

void UMySaveSlotWidget::NativeConstruct()
{
	// 必须调用父类 UUserWidget 的构造逻辑，完成底层 Slate 控件树的搭建
	Super::NativeConstruct();
}

void UMySaveSlotWidget::NativeDestruct()
{
	// 【核心重构】：因为改为在 Initialized 绑定内部组件，这里彻底不需要 RemoveAll 了。
	// 当这块卡片内存被 GC 回收时，内部的按钮会自动死亡，委托自动销毁。
	// 这里只保留一行 Super 调用。

	Super::NativeDestruct();
}

void UMySaveSlotWidget::SetSlotViewModel(UMySaveDataObj* InViewModel)
{
	// MVVM 底座防抖与强制重连
	// 防御性校验（防抖）：检查系统塞进来的“新机顶盒”（ViewModel 实例），是不是卡片当前正在用的那一个。
	// 在双对象池架构中，初次造池子时这里必定不相等；后续复用时可能相等（相等就不需要浪费性能重连底座）。
	if (SlotViewModel != InViewModel)
	{
		// 物理替换：将全新的数据总线（ViewModel）插进这张 UI 卡片的内存槽中
		SlotViewModel = InViewModel;

		// 利用 UHT（虚幻头文件工具）编译时静态生成的 FieldId 标识符，向引擎大声广播：“我的数据源底座换人了！”
		// 底层收到后，会自动去蓝图里找到所有绑定了 SlotViewModel 的 {{表达式}}，并执行一次全量数据异步重绘。
		BroadcastFieldValueChanged(UMySaveSlotWidget::FFieldNotificationClassDescriptor::SlotViewModel);
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
// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/SaveGame/MySaveMenuWidget.h"
#include "UI/SaveGame/MySaveSlotWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h" 
#include "SaveGame/Subsystem/MySaveSubsystem.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"
#include "Engine/World.h"

#include "Components/VerticalBoxSlot.h"

#include "UI/SaveGame/MySaveDataObj.h"

#include "CommonInputBaseTypes.h"


// ==============================================================================
// 核心生命周期与初始化 (Core Lifecycle & Initialization)
// ==============================================================================
#pragma region

UMySaveMenuWidget::UMySaveMenuWidget()
{
	// 在子类的构造函数中，强行覆盖基类的默认值
	// 不允许通过鼠标点击空地来取消 UI
	bCanBeClosedByBackgroundClick = false;

	// 吞噬没经过 UI 拦截的白名单的键盘按键输入
	bAutoStealFocusWhenActivated = true;
}

void UMySaveMenuWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
}

void UMySaveMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 1. 绑定分页按钮的点击事件
	// 为什么用委托绑定：解耦、一对多、动态绑定参与反射系统防止空指针崩溃
	if (Btn_PrevPage) Btn_PrevPage->OnClicked.AddDynamic(this, &UMySaveMenuWidget::OnPrevPageClicked);
	if (Btn_NextPage) Btn_NextPage->OnClicked.AddDynamic(this, &UMySaveMenuWidget::OnNextPageClicked);
	if (Btn_AddPage) Btn_AddPage->OnClicked.AddDynamic(this, &UMySaveMenuWidget::OnAddPageClicked);

	// 绑定清空与整理按钮
	if (Btn_ClearPage) Btn_ClearPage->OnClicked.AddDynamic(this, &UMySaveMenuWidget::OnClearPageClicked);
	if (Btn_CompactPages) Btn_CompactPages->OnClicked.AddDynamic(this, &UMySaveMenuWidget::OnCompactPagesClicked);

	// 2. 找到全局的存档大管家，戴上耳机监听它的广播
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMySaveSubsystem* SaveSub = GI->GetSubsystem<UMySaveSubsystem>())
		{
			// 监听频道A：存盘是否成功（成功了才去播放动画、恢复文本状态）
			SaveSub->OnSaveFinished.AddDynamic(this, &UMySaveMenuWidget::HandleSaveFinished);

			// 监听频道B：注册表是否发生变化（有人删档、扩充页数或存新档）
			// 只要大管家喊“删档了”，这里全自动调用极速重建函数，实现数据的绝对同步响应！
			SaveSub->OnSaveRegistryChanged.AddDynamic(this, &UMySaveMenuWidget::BuildSaveSlotList);
		}
	}

	// 3. 物理打地基：生成且仅生成 5 个卡片，扔进池子里，锁死导航防线
	InitializeSlotPool();

	// 4. 面板刚打开时，主动去拉取一次当前已有的存档列表进行显示
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
// CommonUI 核心重写 (CommonUI Overrides)
// ==============================================================================
#pragma region

TOptional<FUIInputConfig> UMySaveMenuWidget::GetDesiredInputConfig() const
{
	// 参数1: Menu(仅UI模式)  参数2: 不捕获鼠标  参数3: bHideCursor(是否隐藏鼠标) -> 绝对填 false!
	// 这行代码将彻底粉碎 CommonUI 默认隐藏鼠标的霸王条款
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture, false);
}

UWidget* UMySaveMenuWidget::NativeGetDesiredFocusTarget() const
{
	// 【终极破局】：直接把焦点甩给第一张“卡片整体”，而不是里面的“按钮”
	// 因为你在蓝图里把卡片根节点设为了 Focusable=True，它绝对能接住焦点，死循环彻底被掐断！
	if (SlotPool.IsValidIndex(0) && SlotPool[0])
	{
		return SlotPool[0];
	}

	// 兜底方案
	if (Btn_NextPage && Btn_NextPage->GetVisibility() == ESlateVisibility::Visible)
	{
		return Btn_NextPage;
	}

	return Super::NativeGetDesiredFocusTarget();
}

#pragma endregion


// ==============================================================================
// 绝对排版与分页系统 (Absolute Layout & Pagination)
// ==============================================================================
#pragma region

void UMySaveMenuWidget::InitializeSlotPool()
{
	if (!Box_SaveSlots || !SlotWidgetClass) return;

	Box_SaveSlots->ClearChildren();
	SlotPool.Empty();
	ViewModelPool.Empty(); // 【新增】：清空数据池

	// 1. 强行在堆内存中生成并渲染 5 个卡片，并绑定 5 个专属 ViewModel
	for (int32 i = 0; i < SlotsPerPage; ++i)
	{
		if (UMySaveSlotWidget* SlotWidget = CreateWidget<UMySaveSlotWidget>(GetOwningPlayer(), SlotWidgetClass))
		{
			// 拿到 AddChildToVerticalBox 返回的插槽指针
			UVerticalBoxSlot* VBoxSlot = Box_SaveSlots->AddChildToVerticalBox(SlotWidget);

			// 安全校验并强行注入 UI 排版法则
			if (VBoxSlot)
			{
				// 强制把 C++ 动态生成的卡片尺寸设为 Fill（填充），权重默认 1.0
				VBoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			}

			// 【双对象池架构】：在内存中生成一个永久存活的数据总线
			UMySaveDataObj* ViewModel = NewObject<UMySaveDataObj>(this);
			ViewModelPool.Add(ViewModel);

			// 将数据线插进卡片里
			SlotWidget->SetSlotViewModel(ViewModel);

			SlotPool.Add(SlotWidget);
		}
	}

	// 2. 构造显式导航防线 (Explicit Navigation)，防止跨页焦点迷失
	for (int32 i = 0; i < SlotPool.Num(); ++i)
	{
		// 往上推摇杆：如果不是第一个，焦点强行交给上面的卡片；如果是第一个，往上推则保持不动(Stop)
		if (i > 0)
		{
			SlotPool[i]->SetNavigationRuleExplicit(EUINavigation::Up, SlotPool[i - 1]);
		}
		else
		{
			SlotPool[i]->SetNavigationRuleBase(EUINavigation::Up, EUINavigationRule::Stop);
		}

		// 往下推摇杆：如果不是最后一个，焦点强行交给下面的卡片；如果是最后一个，往下推则保持不动(Stop)
		if (i < SlotPool.Num() - 1)
		{
			SlotPool[i]->SetNavigationRuleExplicit(EUINavigation::Down, SlotPool[i + 1]);
		}
		else
		{
			SlotPool[i]->SetNavigationRuleBase(EUINavigation::Down, EUINavigationRule::Stop);
		}
	}
}

void UMySaveMenuWidget::BuildSaveSlotList()
{
	UMySaveSubsystem* SaveSub = GetGameInstance()->GetSubsystem<UMySaveSubsystem>();
	if (!SaveSub || !SaveSub->CachedRegistry || SlotPool.Num() == 0 || ViewModelPool.Num() == 0) return;

	int32 TotalPages = SaveSub->CachedRegistry->UnlockedPages;
	if (CurrentPage > TotalPages) CurrentPage = TotalPages;
	if (CurrentPage < 1) CurrentPage = 1;

	// 算出当前页的第一张卡片编号 (如第1页是从第1个开始，第3页是从第11个开始)
	int32 StartIndex = (CurrentPage - 1) * SlotsPerPage + 1;

	// 【核心降维打击】：不再销毁和重建 UI，只是机械地拿着底层数据，给双对象池中的数据池注水
	for (int32 i = 0; i < SlotPool.Num(); ++i)
	{
		int32 SlotIndex = StartIndex + i;
		FString CurrentSlotName = FString::Printf(TEXT("SaveSlot_%03d"), SlotIndex);

		// O(1) 查表：从大管家的内存镜像中砸门
		FSaveSlotMetaData* FoundMeta = SaveSub->CachedRegistry->SaveSlots.Find(CurrentSlotName);

		// 取出专属 ViewModel 走纯数据同步
		UMySaveDataObj* ViewModel = ViewModelPool[i];

		// ViewModel 的 Setter 会利用 FFieldNotificationId 自动广播，完成局部刷新
		ViewModel->SetSlotName(CurrentSlotName);

		if (FoundMeta)
		{
			ViewModel->SetMetaData(*FoundMeta);
			ViewModel->SetIsEmptySlot(false);
		}
		else
		{
			ViewModel->SetMetaData(FSaveSlotMetaData());
			ViewModel->SetIsEmptySlot(true);
		}

		// 主动同步红节点确保兼容性
		SlotPool[i]->SetSlotViewModel(ViewModel);
	}

	RefreshPaginationUI(TotalPages);
}

void UMySaveMenuWidget::RefreshPaginationUI(int32 TotalPages)
{
	if (Text_PageInfo)
	{
		Text_PageInfo->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), CurrentPage, TotalPages)));
	}

	// 无缝循环分页，上一页和下一页按钮永远保持可用
	if (Btn_PrevPage) Btn_PrevPage->SetIsEnabled(true);
	if (Btn_NextPage) Btn_NextPage->SetIsEnabled(true);

	if (Btn_AddPage)
	{
		Btn_AddPage->SetVisibility(TotalPages < MaxAllowedPages ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UMySaveMenuWidget::OnPrevPageClicked()
{
	UMySaveSubsystem* SaveSub = GetGameInstance()->GetSubsystem<UMySaveSubsystem>();
	if (!SaveSub || !SaveSub->CachedRegistry) return;

	int32 TotalPages = SaveSub->CachedRegistry->UnlockedPages;

	CurrentPage--;
	// 【循环翻页】：如果退到了第 0 页，直接变成最后 1 页
	if (CurrentPage < 1)
	{
		CurrentPage = TotalPages;
	}

	BuildSaveSlotList(); // 重新注水
}

void UMySaveMenuWidget::OnNextPageClicked()
{
	UMySaveSubsystem* SaveSub = GetGameInstance()->GetSubsystem<UMySaveSubsystem>();
	if (!SaveSub || !SaveSub->CachedRegistry) return;

	int32 TotalPages = SaveSub->CachedRegistry->UnlockedPages;

	CurrentPage++;
	// 【循环翻页】：如果超过了总页数，直接回到第 1 页
	if (CurrentPage > TotalPages)
	{
		CurrentPage = 1;
	}

	BuildSaveSlotList(); // 重新注水
}

void UMySaveMenuWidget::OnAddPageClicked()
{
	if (UMySaveSubsystem* SaveSub = GetGameInstance()->GetSubsystem<UMySaveSubsystem>())
	{
		// 记录新的页数并强制翻页过去，随后大管家存盘完毕的广播会自动触发 BuildSaveSlotList
		CurrentPage = SaveSub->CachedRegistry->UnlockedPages + 1;
		SaveSub->UnlockNewSavePage();
	}
}

void UMySaveMenuWidget::OnClearPageClicked()
{
	if (UMySaveSubsystem* SaveSub = GetGameInstance()->GetSubsystem<UMySaveSubsystem>())
	{
		// 仅仅清空数据，页码不会变，所以不需要重置 CurrentPage
		SaveSub->ClearSavePage(CurrentPage);
	}
}

void UMySaveMenuWidget::OnCompactPagesClicked()
{
	if (UMySaveSubsystem* SaveSub = GetGameInstance()->GetSubsystem<UMySaveSubsystem>())
	{
		// 执行高强度的底层 I/O 整理
		SaveSub->CompactEmptySavePages();

		// 如果因为碎片整理导致总页数变少，且玩家刚好处于被砍掉的页数上，自动将视角拉回最后一页
		if (CurrentPage > SaveSub->CachedRegistry->UnlockedPages)
		{
			CurrentPage = SaveSub->CachedRegistry->UnlockedPages;
		}
		if (CurrentPage < 1) CurrentPage = 1;

		// 【焦点残留 Bug 修复】：整理完数据后，必须立刻强行更新当前页列表，防止卡片数据漂移
		BuildSaveSlotList();

		// 强行把焦点锁回第一个槽位，消除 (Elimination) 玩家在使用手柄整理存档后，焦点莫名其妙消失的严重 Bug
		if (SlotPool.IsValidIndex(0))
		{
			SlotPool[0]->SetFocus();
		}
	}
}

#pragma endregion

// ==============================================================================
// 全局状态监听 (Global State)
// ==============================================================================
#pragma region

void UMySaveMenuWidget::HandleSaveFinished(bool bSuccess)
{
	// 收到大管家传回来的异步写盘结果
	if (bSuccess)
	{
		// 触发蓝图动画，给玩家一个酷炫的视觉反馈
		BP_PlaySaveSuccessAnimation();
	}
	else
	{
		// 极低概率触发（硬盘满、权限不足）的错误托底

	}
}

#pragma endregion
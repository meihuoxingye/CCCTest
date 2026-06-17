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
// 绝对排版与分页系统 (Absolute Layout & Pagination)
// ==============================================================================
#pragma region

void UMySaveMenuWidget::InitializeSlotPool()
{
	if (!Box_SaveSlots || !SlotWidgetClass) return;

	Box_SaveSlots->ClearChildren();
	SlotPool.Empty();

	// 1. 强行在堆内存中生成并渲染 5 个卡片
	for (int32 i = 0; i < SlotsPerPage; ++i)
	{
		if (UMySaveSlotWidget* SlotWidget = CreateWidget<UMySaveSlotWidget>(GetOwningPlayer(), SlotWidgetClass))
		{
			Box_SaveSlots->AddChildToVerticalBox(SlotWidget);
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
	if (!SaveSub || !SaveSub->CachedRegistry || SlotPool.Num() == 0) return;

	int32 TotalPages = SaveSub->CachedRegistry->UnlockedPages;
	if (CurrentPage > TotalPages) CurrentPage = TotalPages;
	if (CurrentPage < 1) CurrentPage = 1;

	// 算出当前页的第一张卡片编号 (如第1页是从第1个开始，第3页是从第11个开始)
	int32 StartIndex = (CurrentPage - 1) * SlotsPerPage + 1;

	// 【核心降维打击】：不再销毁和重建 UI，只是机械地拿着底层数据，给池子里的 5 个卡片重新注水
	for (int32 i = 0; i < SlotPool.Num(); ++i)
	{
		int32 SlotIndex = StartIndex + i;
		FString CurrentSlotName = FString::Printf(TEXT("SaveSlot_%03d"), SlotIndex);

		// O(1) 查表：从大管家的内存镜像中砸门
		FSaveSlotMetaData* FoundMeta = SaveSub->CachedRegistry->SaveSlots.Find(CurrentSlotName);

		// 直接呼叫卡片的 Init 函数，卡片内部会自动切成“空档”或“满档”显示
		SlotPool[i]->InitSlotData(CurrentSlotName, FoundMeta);
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
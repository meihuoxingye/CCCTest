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

#include "UI/CommonButton/MyCommonButtonBase.h" 


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

void UMySaveMenuWidget::NativeOnInitialized()
{
	// 1. 绑定分页按钮的点击事件
	// 为什么用委托绑定：解耦、一对多、动态绑定参与反射系统防止空指针崩溃
	if (Btn_PrevPage) Btn_PrevPage->OnClicked().AddUObject(this, &UMySaveMenuWidget::OnPrevPageClicked);
	if (Btn_NextPage) Btn_NextPage->OnClicked().AddUObject(this, &UMySaveMenuWidget::OnNextPageClicked);
	if (Btn_AddPage) Btn_AddPage->OnClicked().AddUObject(this, &UMySaveMenuWidget::OnAddPageClicked);

	// 绑定清空与整理按钮
	if (Btn_ClearPage) Btn_ClearPage->OnClicked().AddUObject(this, &UMySaveMenuWidget::OnClearPageClicked);
	if (Btn_CompactPages) Btn_CompactPages->OnClicked().AddUObject(this, &UMySaveMenuWidget::OnCompactPagesClicked);

	// 2. 找到全局的存档大管家，戴上耳机监听它的广播；缓存每页档位数、最高页数上限
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMySaveSubsystem* SaveSub = GI->GetSubsystem<UMySaveSubsystem>())
		{
			// 监听频道A：存盘是否成功（成功了才去播放动画、恢复文本状态）
			SaveSub->OnSaveFinished.AddDynamic(this, &UMySaveMenuWidget::HandleSaveFinished);

			// 监听频道B：注册表是否发生变化（有人删档、扩充页数或存新档）
			// 只要大管家喊“删档了”，这里全自动调用极速重建函数，实现数据的绝对同步响应！
			SaveSub->OnSaveRegistryChanged.AddDynamic(this, &UMySaveMenuWidget::BuildSaveSlotList);

			// 缓存每页档位数
			CachedSlotsPerPage = SaveSub->GetSlotsPerPage();
			// 缓存最高页数上限
			CachedMaxUnlockedPages = SaveSub->GetMaxUnlockedPages();
		}
	}

	// 3. 物理打地基：生成且仅生成 5 个卡片，扔进池子里，锁死导航防线
	InitializeSlotPool();
}

void UMySaveMenuWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
}

void UMySaveMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 面板刚打开时，主动去拉取一次当前已有的存档列表进行显示
	BuildSaveSlotList();
}

void UMySaveMenuWidget::NativeDestruct()
{
	// 【核心重构】：大管家绑定已移至 Initialized (一生只绑一次)。
	// 绝对禁止在此处使用 RemoveDynamic，否则第二次打开 UI 时将永久失去监听！
	// Dynamic 委托底层自带弱指针保护，UI 被 GC 销毁时绝对不会引发野指针崩溃。

	Super::NativeDestruct();
}

#pragma endregion


// ==============================================================================
// CommonUI 核心重写 (CommonUI Overrides)
// ==============================================================================
#pragma region

TOptional<FUIInputConfig> UMySaveMenuWidget::GetDesiredInputConfig() const
{
	// 参数1: All (GameAndUI) 模式  参数2: 不捕获鼠标  参数3: bHideCursor(是否隐藏鼠标) -> 绝对填 false!
	// 这行代码将彻底粉碎 CommonUI 默认隐藏鼠标的霸王条款

	// All (GameAndUI) 模式：如果 UI 层面不要这个键（Unhandled），那就把它顺理成章地扔给底层的 3D 角色！
	// NoCapture(不捕获) 鼠标滑到屏幕边缘时，可以直接划出游戏窗口，划到你的 Windows 桌面或者第二块显示器上。
	return FUIInputConfig(ECommonInputMode::All, EMouseCaptureMode::NoCapture, false);
}

UWidget* UMySaveMenuWidget::NativeGetDesiredFocusTarget() const
{
	// 第一防线（首选体验）：光标吸附在第 1 张存档卡片上（玩家最舒服的起点）。
	// 【终极破局】：直接把焦点甩给第一张“卡片整体”，而不是里面的“按钮”
	// 因为你在蓝图里把卡片根节点设为了 Focusable=True，它绝对能接住焦点，死循环彻底被掐断！
	if (SlotPool.IsValidIndex(0) && SlotPool[0])
	{
		return SlotPool[0];
	}

	// 第二防线（物理兜底）：如果卡片挂了，光标吸附在“下一页”按钮上（保证手柄 / 键盘玩家绝对有的选，不会死锁）。
	if (Btn_NextPage && Btn_NextPage->GetVisibility() == ESlateVisibility::Visible)
	{
		return Btn_NextPage;
	}

	// 第三防线（引擎原生）：交给 Super 处理。
	return Super::NativeGetDesiredFocusTarget();
}

void UMySaveMenuWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
}

#pragma endregion


// ==============================================================================
// 绝对排版与分页系统 (Absolute Layout & Pagination)
// ==============================================================================
#pragma region

void UMySaveMenuWidget::InitializeSlotPool()
{
	// 绝对防御：查验蓝图是否正确绑定了垂直容器与卡片类模板，严防后续生成时引发空指针崩溃。
	if (!Box_SaveSlots || !SlotWidgetClass) return;

	// 物理清场：强行抹除底层 Slate 容器中可能残留的旧 UI 节点。
	Box_SaveSlots->ClearChildren();

	// 引用清零：彻底清空“UI实体”与“MVVM数据”双对象池的旧指针，防止重复调用导致内存泄漏与状态重叠。
	SlotPool.Empty();
	ViewModelPool.Empty();

	// 1. 强行在堆内存中生成并渲染 5 个卡片，并绑定 5 个专属 ViewModel
	for (int32 i = 0; i < CachedSlotsPerPage; ++i)
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
	// 底层原理：彻底废除 Slate 引擎默认的“空间射线寻路”机制。引擎会向周围发射射线来找下一个按钮；
	// 在手柄/键盘交互中，默认的射线寻路极易因边缘碰撞或 UI 重叠，导致焦点飞出列表容器外引发假死。
	// 强硬手段：用内存地址将 5 张卡片串成一条“内部锁死、两端开放”的物理铁链，确保内部上下导航绝对精准，首尾接线权交由蓝图自由外接其他按钮。
	for (int32 i = 0; i < SlotPool.Num(); ++i)
	{
		// 【向上导航法则】：只要当前不是第 1 张卡片（索引大于 0）
		if (i > 0)
		{
			// 内部向上串联：强制规定往上推摇杆时，光标直接跳到数组里的前一个卡片（i - 1）。
			SlotPool[i]->SetNavigationRuleExplicit(EUINavigation::Up, SlotPool[i - 1]);
		}

		// 【向下导航法则】：只要当前不是最后 1 张卡片（索引小于总数减 1）
		if (i < SlotPool.Num() - 1)
		{
			// 内部向下串联：强制规定往下推摇杆时，光标直接跳到数组里的下一个卡片（i + 1）。
			SlotPool[i]->SetNavigationRuleExplicit(EUINavigation::Down, SlotPool[i + 1]);
		}
	}

	// 3. 边界移交：呼叫蓝图事件，将“上下逃逸口”的接线权移交给表现层
	// 只要生成了卡片，就把排在头尾的两位代表扔出去
	if (SlotPool.Num() > 0)
	{
		// 将首、尾两张卡片当做参数传给蓝图，让表现层自己去连“第一张往上”和“最后一张往下”该去哪个外围按钮。
		BP_SetupBoundaryNavigation(SlotPool[0], SlotPool.Last());
	}
}

void UMySaveMenuWidget::BuildSaveSlotList()
{
	// 从全局游戏实例中提取存档子系统大管家的指针。
	UMySaveSubsystem* SaveSub = GetGameInstance()->GetSubsystem<UMySaveSubsystem>();

	// 绝对防御：如果大管家阵亡、内存台账丢失，或双对象池未能在 UI 构建阶段成功蓄满，则立刻阻断执行防崩溃。
	if (!SaveSub || !SaveSub->CachedRegistry || SlotPool.Num() == 0 || ViewModelPool.Num() == 0) return;

	// 从内存台账中读取当前玩家已经解锁的最大存档页数。
	int32 TotalPages = SaveSub->GetTotalUnlockedPages();

	// 页码越界钳制（上限）：如果当前翻页超出了最大解锁页数，强行拉回最后一页。
	if (CurrentPage > TotalPages) CurrentPage = TotalPages;

	// 页码越界钳制（下限）：如果当前页码被错误地减到了小等于 0，强行重置回第 1 页。
	if (CurrentPage < 1) CurrentPage = 1;

	// 算出当前页的第一张卡片编号 (如第1页是从第1个开始，第3页是从第11个开始)
	int32 StartIndex = (CurrentPage - 1) * CachedSlotsPerPage + 1;

	// 【核心降维打击】：不再销毁和重建 UI，只是机械地拿着底层数据，给双对象池中的数据池注水
	// 遍历那 5 个永远不死、死死钉在墙上的物理 UI 卡片池。
	for (int32 i = 0; i < SlotPool.Num(); ++i)
	{
		// 计算出当前这轮循环所对应的存档物理绝对序号（如 6, 7, 8, 9, 10）。
		int32 SlotIndex = StartIndex + i;

		// 将物理序号格式化为标准的内部识别名字符串（例如 "SaveSlot_006"）。
		FString CurrentSlotName = FString::Printf(TEXT("SaveSlot_%03d"), SlotIndex);

		// O(1) 查表：从大管家的内存镜像中砸门
		// 拿着刚算出的识别名，去内存台账的哈希表字典里极速查找匹配（绝不在此刻去读取物理硬盘）。
		FSaveSlotMetaData* FoundMeta = SaveSub->CachedRegistry->SaveSlots.Find(CurrentSlotName);

		// 取出专属 ViewModel 走纯数据同步
		// 从数据池中，抽出跟当前这张 UI 卡片一对一物理绑死的底层机顶盒。
		UMySaveDataObj* ViewModel = ViewModelPool[i];

		// ViewModel 的 Setter 会利用 FFieldNotificationId 自动广播，完成局部刷新
		// 将新算出的存档名称直接灌入机顶盒，底层立刻触发精准的局部重绘。
		// 不在 if 里，即使是空档位也必须要有门牌号。
		ViewModel->SetSlotName(CurrentSlotName);

		// 状态分叉点：如果哈希字典里查到了这个档位的数据（说明这不是一个空档）。
		if (FoundMeta)
		{
			// 将查到的真实存档元数据（时间、进度等结构体）硬塞进机顶盒里更新。
			ViewModel->SetMetaData(*FoundMeta);

			// 标记机顶盒状态为“非空”，前端 UI 侦测到后会自动显示进度条，隐藏“新建存档”按钮。
			ViewModel->SetIsEmptySlot(false);
		}
		// 反之，如果字典里没查到（说明这是一个从没存过进度的空档位）。
		else
		{
			// 强行塞入一个全新的、默认清零的空结构体，用来抹除上一页残余在机顶盒里的旧档脏数据。
			ViewModel->SetMetaData(FSaveSlotMetaData());

			// 标记机顶盒状态为“空壳”，前端 UI 会立刻隐藏进度条，露出“新建存档”大字。
			ViewModel->SetIsEmptySlot(true);
		}

		// 复杂的材质渐变、UMG 动画播放，纯靠 MVVM 数据绑定是极难实现的。
		// SetSlotViewModel 通常是一个暴露给蓝图的接口（或者它内部会触发一个蓝图事件）。
		// 【强制重连与表现激活】：防脱钩的最后一道保险
		// 动作 1（防假死）：强制把新机顶盒插入电视机。触发底层重置 MVVM 绑定上下文，防止 UI 因对象池高频复用而产生“数据换了但画面没变”的假死 Bug。
		// 动作 2（赋表现）：主动呼叫表现层蓝图（红节点），把动画、材质切换（如空档变灰、有档发光）的控制权全权交还给 UI 设计师。
		SlotPool[i]->SetSlotViewModel(ViewModel);
	}

	// 核心数据注水完毕，最后呼叫外围系统，根据页码状态重新判定“上一页/下一页”按钮是否需要置灰。
	RefreshPaginationUI(TotalPages);
}

void UMySaveMenuWidget::RefreshPaginationUI(int32 TotalPages)
{
	// 安全校验：确保负责显示页码的文本控件指针不为空，严防 UI 蓝图中手滑没绑定该控件导致的空指针崩溃。
	if (Text_PageInfo)
	{
		// 字符串拼接与渲染：将当前的页码状态格式化为类似 "1 / 5" 的直观文本，并强制推送到前端屏幕上。
		Text_PageInfo->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), CurrentPage, TotalPages)));
	}

	// 核心交互逻辑：【无缝循环分页】。
	// 因为系统允许玩家在第 1 页往前翻直接跳到最后 1 页，所以翻页键永远不需要被 Disable（置灰）。

	// 安全校验并强制激活“上一页”按钮的交互状态。
	if (Btn_PrevPage) Btn_PrevPage->SetIsEnabled(true);

	// 安全校验并强制激活“下一页”按钮的交互状态。
	if (Btn_NextPage) Btn_NextPage->SetIsEnabled(true);

	// 扩充容量机制的视觉表现控制
	// 安全校验：确保“解锁新页”按钮的指针存在。
	if (Btn_AddPage)
	{
		// 极限容量钳制：判断大管家台账里的当前总页数，是否还没碰到游戏设定的绝对硬上限 (MaxAllowedPages)。
		// 三元运算符判定：
		// 如果还没满，设为 Visible，让玩家可以看见并点击继续购买新页；
		// 如果已经扩充到极限，直接设为 Collapsed，不仅让按钮隐形，还会将其从 Slate 底层排版树中彻底抽离，不占任何空间。
		Btn_AddPage->SetVisibility(TotalPages < CachedMaxUnlockedPages ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UMySaveMenuWidget::OnPrevPageClicked()
{
	// 获取存档大管家指针。
	UMySaveSubsystem* SaveSub = GetGameInstance()->GetSubsystem<UMySaveSubsystem>();

	// 绝对防御防线：如果大管家阵亡或底层内存镜像台账丢失，立刻阻断指令，严防越界与空指针崩溃。
	if (!SaveSub || !SaveSub->CachedRegistry) return;

	// O(1) 极速调阅：绝不去碰硬盘，直接从内存镜像中读取当前已解锁的最大存档页数。
	int32 TotalPages = SaveSub->GetTotalUnlockedPages();

	// 游标逻辑位移：将当前所在页码减 1。
	CurrentPage--;

	// 【循环翻页】：如果退到了第 0 页，直接变成最后 1 页
	if (CurrentPage < 1)
	{
		CurrentPage = TotalPages;
	}

	// 重新注水
	BuildSaveSlotList();
}

void UMySaveMenuWidget::OnNextPageClicked()
{
	// 获取存档大管家指针。
	UMySaveSubsystem* SaveSub = GetGameInstance()->GetSubsystem<UMySaveSubsystem>();

	// 绝对防御防线：如果大管家阵亡或底层内存镜像台账丢失，立刻阻断指令，严防越界与空指针崩溃。
	if (!SaveSub || !SaveSub->CachedRegistry) return;

	// O(1) 极速调阅：绝不去碰硬盘，直接从内存镜像中读取当前已解锁的最大存档页数。
	int32 TotalPages = SaveSub->GetTotalUnlockedPages();

	// 游标逻辑位移：将当前所在页码加 1。
	CurrentPage++;

	// 【循环翻页】：如果超过了总页数，直接回到第 1 页
	if (CurrentPage > TotalPages)
	{
		CurrentPage = 1;
	}

	// 重新注水
	BuildSaveSlotList(); 
}

void UMySaveMenuWidget::OnAddPageClicked()
{
	if (UMySaveSubsystem* SaveSub = GetGameInstance()->GetSubsystem<UMySaveSubsystem>())
	{
		// 记录新的页数并强制翻页过去，随后大管家存盘完毕的广播会自动触发 BuildSaveSlotList
		CurrentPage = SaveSub->GetTotalUnlockedPages() + 1;
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
		// 执行高强度的底层 I/O 存档碎片整理
		SaveSub->CompactEmptySavePages();

		// 获取当前玩家已解锁的总页数
		int32 CurrentTotalPages = SaveSub->GetTotalUnlockedPages();

		// 如果玩家当前停留的页码，超出了整理后的新总页数（即游标溢出），
		// 必须强制将视线拉回新的最后一页，防止后续生成 UI 时引发数组越界。
		if (CurrentPage > CurrentTotalPages)
		{
			CurrentPage = CurrentTotalPages;
		}

		// 【焦点残留 Bug 修复】：整理完数据后，必须立刻强行更新当前页列表，防止卡片数据漂移
		BuildSaveSlotList();
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
// Fill out your copyright notice in the Description page of Project Settings.

// 引入当前类的头文件
#include "UI/ActivatableWidget/MyActivatableWidgetBase.h"
// 引入玩法静态函数库（如处理输入、获取全局对象等）
#include "Kismet/GameplayStatics.h"
// 引入 Slate 核心的输入响应机制结构体
#include "Input/Reply.h"
// 引入处理键盘、鼠标等输入事件的结构体
#include "Input/Events.h"
// 【新增】：为了获取 LocalPlayer 和 UI 子系统
// 引入本地玩家对象头文件，用于获取所属玩家数据
#include "Engine/LocalPlayer.h"
// 引入自定义的 UI 管理器子系统头文件
#include "UI/Subsystem/MyUIManagerSubsystem.h"
// 必须包含增强输入相关头文件
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "EnhancedActionKeyMapping.h" // 【修正 4】：必须包含此文件才能访问 Mapping.Action 和 Mapping.Key

#include "Framework/Application/SlateApplication.h"


// ==============================================================================
// 状态驱动接口 (State Drivers)
// ==============================================================================
#pragma region

// 【激活接口】：由外部调用，触发 UI 展开逻辑，展开过程中只执行一次。
// 【C++ 底层保底逻辑 (_Implementation)】：
// 1. 状态拦截：若已在展开中则直接忽略，防止玩家狂按快捷键导致动画重置。
// 2. 数据归零：重置进度 TransitionProgress 为 0.0，设为可见，切入 Opening 状态。
// 3. 拦截网入栈：主动寻址 UMyUIManagerSubsystem 并将自身 Push 入栈，瞬间接管全局输入焦点。
// 4. 终极防闪烁：执行 ForceLayoutPrepass() 强迫引擎在蓝图渲染前提前算好 Slate 尺寸。
// 5. 点火起飞：计算首帧缓动进度，触发 UpdateOpeningEffect 钩子，唤醒蓝图动画。
void UMyActivatableWidgetBase::OnWidgetActivated_Implementation()
{
	// 【新增优化】：拦截“虚空编辑器”逻辑，防止 UMG 编辑器在预览/编译时尝试运行底层代码，彻底杜绝缺失上下文引发的崩溃
	if (IsDesignTime()) return;

	// 如果已经在打开的过程中，直接返回，避免重复触发激活逻辑
	if (CurrentState == EUIState::Opening) return;

	// 如果当前是折叠不可见状态，或者进度已经归零（完全关闭状态）
	if (GetVisibility() == ESlateVisibility::Collapsed || TransitionProgress <= 0.0f)
	{
		// 重置进度到 0.0，准备重新播放入场动画
		TransitionProgress = 0.0f;
	}

	// ======================================================================
	// 【极简核心】：物理级刷新输入缓存
	// 彻底消除切人键在 UI 激活瞬间因“加载时差”被误吞的 Bug。
	// ======================================================================
	RefreshInputPassthroughCache();

	// 将 UI 的根节点（WBP_TacticalMenu）设为可见，从而使整个UI可见，使其开始渲染并接受交互
	// 因为根节点下就是画布面板，所以实际上整个屏幕都被设为可见
	// 为什么不只设置 UI 实际区域可见，其他部分可直接穿透到游戏地面？
	// 因为如果 UI 区域以外的部分不可见了，那么玩家点击屏幕其他地方时，输入事件就会穿透到游戏世界里，导致玩家误操作
	// 需要 UI 区域以外的部分参与检测，以确定点击到底是控制 UI 还是想要退出 UI
	SetVisibility(ESlateVisibility::Visible);

	// --------------------------------------------------------------------------
	// 【核心改变 / 终极防闪烁装甲】：在将控制权交给蓝图之前，强行打断引擎队列，立刻执行底层排版！
	// 只有在此刻强制算出 Geometry，后续的夺焦逻辑才能因为有了“物理实体”而 100% 成功。
	// --------------------------------------------------------------------------
	// [底层机制补充说明]：
	// 1. Slate 的惰性排版 (Lazy Layout)：为了极致性能，引擎在 AddToViewport 或 SetVisibility(Visible) 时，
	//    并不会立刻计算 UI 的长宽大小，而是将其塞入“待办队列”，等到下一帧渲染前才统一计算。
	// 2. 致命的零维度陷阱：如果我们不打断这个队列，紧接着在下一行直接触发 UpdateOpeningEffect 动画，
	//    蓝图节点 `Get Paint Space Geometry` 拿到的尺寸将是致命的 (0, 0)！
	// 3. 视觉灾难 (1-Frame Flicker)：尺寸为 0 会导致基于尺寸推演的进场动画（如从屏幕最下方滑入）在第 0 帧彻底算错位置，
	//    让 UI 在屏幕左上角像幽灵一样“闪烁一帧”，随后才跳回正确位置。
	// 4. 破局：调用 ForceLayoutPrepass()，相当于拿枪指着引擎的头，逼迫它在当前这行 C++ 走完之前，立刻、同步地算出绝对尺寸！
	ForceLayoutPrepass();

	// ======================================================================
	// 【绝对权力夺焦】：在排版完成后，趁热打铁焊死焦点，彻底消除“点一下”的 Bug
	// ======================================================================
	bIsFocusable = true;
	SetIsFocusable(true); // 确保当前 UI 具备获取焦点的资格

	// 强制打开“接纳焦点”的物理通路，无视蓝图里漏勾选的配置。
	if (UWidget* RootNode = GetRootWidget())
	{
		this->bIsFocusable = true;
	}

	if (APlayerController* PC = GetOwningPlayer())
	{
		// 清空所有硬件按键的按下状态，彻底解决玩家按住键呼出菜单导致的“按键粘连”Bug
		PC->FlushPressedKeys();

		FInputModeGameAndUI InputMode;
		// 此时 GetCachedWidget() 已经通过了上方的 Prepass 获得了真实数据，绝对有效
		TSharedPtr<SWidget> SlateWidget = GetCachedWidget();
		if (!SlateWidget.IsValid()) SlateWidget = TakeWidget();

		InputMode.SetWidgetToFocus(SlateWidget);
		// 保证鼠标不会被锁在屏幕中心
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		// 强制在视口捕获期间也不隐藏光标，防止点击空白处时鼠标突然消失
		InputMode.SetHideCursorDuringCapture(false);

		PC->SetInputMode(InputMode);
		// 配合 GameAndUI 模式，明确显示鼠标指针
		PC->SetShowMouseCursor(true);
	}

	// 强制调用原生聚焦，将硬件键盘死死钉在 UI 上
	SetFocus();

	// 将状态机切换到“正在打开”
	CurrentState = EUIState::Opening;

	// 【全自动入栈】
	// 获取当前 UI 所属的本地玩家
	// 1. 作用域收束 (C++17)：将 LP 和 UIMgr 的声明锁死在 if 块内，物理级别根绝野指针泄露。
	// 2. 拒绝硬编码：绝对禁用 GetPlayerController(0)！通过 GetOwningLocalPlayer 动态寻址，使该 UI 框架天生完美兼容“本地双人分屏/同屏多打”，永远只获取召唤自己的那个宿主玩家。
	// 3. 数据物理隔离：因 UIMgr 是 LocalPlayerSubsystem，1P 和 2P 各自拥有一套独立的 UI 管理器，互相狂按面板也绝对不会导致对方的 UI 栈错乱。
	if (ULocalPlayer* LP = GetOwningLocalPlayer())
	{
		// 获取对应的 UI 统筹子系统
		if (UMyUIManagerSubsystem* UIMgr = LP->GetSubsystem<UMyUIManagerSubsystem>())
		{
			// 将自身推入拦截栈，接管输入焦点
			UIMgr->PushUI(this);
		}
	}

	// 准备首帧时间参数：因为第一帧的 NativeTick 尚未运行，必须手动利用起跑点计算一次初始参数。
	// 本函数只会在 UI 展开时执行一次，所以此函数里计算的 TransitionProgress 绝对是 0.0f，代表动画起点；
	// 输入实际经过时间占动画总时长的比例 TransitionProgress，根据范围（0.0 - 1.0）与曲率 OpeningExp，
	// 计算出当前实际经过时间对应的缓动进度 EasedProgress，若曲率为 2 则 EasedProgress 会在开始缓慢，而中途加速，快结束时又减速，形成丝滑的飞入感
	float EasedProgress = FMath::InterpEaseInOut(0.0f, 1.0f, TransitionProgress, OpeningExp);
	// 触发更新动画，防止出现一帧的默认状态画面闪烁
	UpdateOpeningEffect(TransitionProgress, EasedProgress);
}

// 【激活接口】：由外部调用，触发 UI 收起逻辑，收起过程中只执行一次。
// 【C++ 底层保底逻辑 (_Implementation)】：
// 1. 状态拦截：若已在收起中，或已经处于隐藏折叠状态，则直接返回。
// 2. 状态切换：将状态机切入 Closing 状态，由 NativeTick 纯数学引擎接管后续的退场动画推演。
// 3. 幽灵点击护盾（极其关键）：此时绝不调用 PopUI 出栈！必须让 UI 留在拦截栈里挡子弹，
// 4. 直到 NativeTick 判定退场动画彻底播完、UI 彻底看不见时，才安全地执行出栈逻辑。
void UMyActivatableWidgetBase::OnWidgetDeactivated_Implementation()
{
	// 如果已经在关闭过程中，或者是折叠不可见状态，则无需重复执行反激活
	if (CurrentState == EUIState::Closing || GetVisibility() == ESlateVisibility::Collapsed) return;

	// 将状态机切换为“正在关闭”
	CurrentState = EUIState::Closing;

	// 注意：这里绝不调用 PopUI！留在拦截栈中抗点击穿透，交由 Tick 动画结束时出栈。
}
#pragma endregion

// ==============================================================================
// 控件生命周期 (Widget Lifecycle)
// ==============================================================================
#pragma region

void UMyActivatableWidgetBase::NativeOnInitialized()
{
	// 调用父类 UserWidget 的初始化逻辑
	Super::NativeOnInitialized();

	// 将 UI 动画过渡进度归零（0.0 代表动画起点）
	TransitionProgress = 0.0f;
	// 将 UI 的初始状态设置为待机（Idle）
	CurrentState = EUIState::Idle;
}

void UMyActivatableWidgetBase::NativeConstruct()
{
	// 1. 执行底层构建：完成 UMG/Slate 控件树的实例化与运行时环境挂载
	Super::NativeConstruct();

	// 2. 作用域收束与安全寻址：精准获取当前 UI 的宿主本地玩家
	// @防线：使用 GetOwningLocalPlayer() 替代 GetPlayerController(0)，物理隔离多实例/分屏环境中的输入越权，且能防止网络同步环境下的空指针崩溃。
	if (ULocalPlayer* LP = GetOwningLocalPlayer())
	{
		// 3. 获取增强输入子系统：定位负责当前玩家输入管线仲裁的局部单例
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			// 4. 挂载缓存重建监听（数据驱动核心）
			// @触发时机：当玩家在设置菜单中修改了快捷键，或者底层添加/移除了输入上下文导致控制逻辑重组时广播。
			// 使用 AddUniqueDynamic 防重绑定，确保底层物理按键发生变动时，UI 的免检名单能实时自我修正
			// 防止玩家改键后“新快捷键失灵”或“旧快捷键误漏”。
			// ControlMappingsRebuiltDelegate，引擎底层专门用来通知全量系统：“玩家的按键映射规则发生改变” 的一个全局大喇叭
			InputSubsystem->ControlMappingsRebuiltDelegate.AddUniqueDynamic(this, &UMyActivatableWidgetBase::RefreshInputPassthroughCache);
		}
	}

	// 5. 初始建仓：在 UI 刚加入屏幕并准备接收首个硬件输入的前一刻，强制执行一次遍历，将允许穿透的按键填入 O(1) 哈希缓存中
	RefreshInputPassthroughCache();
}

void UMyActivatableWidgetBase::SynchronizeProperties()
{
	// 调用父类逻辑，同步蓝图编辑器中的属性到 C++ 实例
	Super::SynchronizeProperties();

	// 如果当前 UI 正在打开或者处于待机状态
	if (CurrentState == EUIState::Opening || CurrentState == EUIState::Idle)
	{
		// 计算经过缓动曲线（EaseInOut）处理后的当前进度值
		float EasedProgress = FMath::InterpEaseInOut(0.0f, 1.0f, TransitionProgress, OpeningExp);
		// 将计算好的进度传递给蓝图或底层的打开特效更新函数
		UpdateOpeningEffect(TransitionProgress, EasedProgress);
	}
	// 如果当前 UI 正在关闭
	else if (CurrentState == EUIState::Closing)
	{
		// 计算经过缓动曲线处理后的关闭动画进度值
		float EasedProgress = FMath::InterpEaseInOut(0.0f, 1.0f, TransitionProgress, ClosingExp);
		// 将计算好的进度传递给蓝图或底层的关闭特效更新函数
		UpdateClosingEffect(TransitionProgress, EasedProgress);
	}
}

void UMyActivatableWidgetBase::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	// 调用父类 Tick，保证基础 UI 逻辑正常执行
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 如果处于待机状态，不需要更新动画进度，直接返回以节省性能
	if (CurrentState == EUIState::Idle) return;

	// 根据当前是打开还是关闭状态，确定对应的动画总时长
	float CurrentDuration = (CurrentState == EUIState::Opening) ? OpeningDuration : ClosingDuration;
	// 计算本帧进度增加/减少的步长（防止除零错误，最少取 0.01f）
	float Step = InDeltaTime / FMath::Max(0.01f, CurrentDuration);

	// 如果正在播放入场动画
	if (CurrentState == EUIState::Opening)
	{
		// 累加进度，最高不超过 1.0f
		TransitionProgress = FMath::Min(TransitionProgress + Step, 1.0f);
	}
	// 如果正在播放退场动画
	else if (CurrentState == EUIState::Closing)
	{
		// 递减进度，最低不低于 0.0f
		TransitionProgress = FMath::Max(TransitionProgress - Step, 0.0f);
	}

	// 再次判断状态，用于应用动画效果及判定动画是否结束
	if (CurrentState == EUIState::Opening)
	{
		// 计算缓动后的进度值
		float EasedProgress = FMath::InterpEaseInOut(0.0f, 1.0f, TransitionProgress, OpeningExp);
		// 更新打开动画的视觉表现
		UpdateOpeningEffect(TransitionProgress, EasedProgress);

		// 如果进度达到或超过 1.0，说明入场动画播放完毕
		if (TransitionProgress >= 1.0f) CurrentState = EUIState::Idle; // 切换为待机状态
	}
	else if (CurrentState == EUIState::Closing)
	{
		// 计算缓动后的进度值
		float EasedProgress = FMath::InterpEaseInOut(0.0f, 1.0f, TransitionProgress, ClosingExp);
		// 更新关闭动画的视觉表现
		UpdateClosingEffect(TransitionProgress, EasedProgress);

		// 状态机收尾：当进度彻底归零（退场动画播完）
		if (TransitionProgress <= 0.0f)
		{
			// 将状态重置为待机
			CurrentState = EUIState::Idle;
			// 隐藏 UI（折叠状态不占位、不渲染）
			SetVisibility(ESlateVisibility::Collapsed);

			// 【核心修复】：直到 UI 彻底看不见了，才将其出栈！
			// 这彻底封死了“退场动画期间开火走火”的幽灵点击 Bug。
			// 获取持有该 UI 的本地玩家实例
			if (ULocalPlayer* LP = GetOwningLocalPlayer())
			{
				// 获取挂载在本地玩家上的自定义 UI 管理器子系统
				if (UMyUIManagerSubsystem* UIMgr = LP->GetSubsystem<UMyUIManagerSubsystem>())
				{
					// 通知管理器将当前 UI 从拦截栈中移除
					UIMgr->PopUI(this);
				}
			}
		}
	}
}

void UMyActivatableWidgetBase::NativeDestruct()
{
	// 最后的保险：无论 UI 是怎么没的（切场景、被强杀等），都要从子系统中消除
	// 获取所属本地玩家
	if (ULocalPlayer* LP = GetOwningLocalPlayer())
	{
		// 获取 UI 子系统
		if (UMyUIManagerSubsystem* UIMgr = LP->GetSubsystem<UMyUIManagerSubsystem>())
		{
			// 强行从子系统栈中移除自己，防止悬空指针
			UIMgr->PopUI(this);
		}

		// 解绑增强输入映射重构委托，防止野指针崩溃
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			// 对于动态多播委托，建议使用对应的 RemoveDynamic，而不是 RemoveAll(this)
			InputSubsystem->ControlMappingsRebuiltDelegate.RemoveDynamic(this, &UMyActivatableWidgetBase::RefreshInputPassthroughCache);
		}
	}

	// 调用父类的析构清理逻辑
	Super::NativeDestruct();
}
#pragma endregion

// ==============================================================================
// 响应式输入路由 (Input Routing)
// ==============================================================================
#pragma region

FReply UMyActivatableWidgetBase::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 【动态动作寻址】：向 O(1) 缓存查询当前按下的鼠标物理键（无论左/右/中/侧键）是否隶属于蓝图配置的免检动作名单。
	if (IsPassthroughAction(InMouseEvent.GetEffectingButton()))
	{
		// 动作命中：主动放弃拦截（Unhandled）。
		// 允许信号穿透当前 UI 护盾，漏向底层游戏场景（例如：允许放行中键以缩放 2.5D 战术视角）。
		return FReply::Unhandled();
	}

	// 动作未命中：在 UI 层安全吞噬该点击（彻底切断穿透，防止玩家点击面板引发底层主角的意外走火），
	// 且能完美保留 UI 内部控件（如关闭按钮、滑动条）被正常点按的交互功能。
	FReply Reply = Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);

	// 【核心护盾修复】：如果父类没有处理（比如点在了 UI 的空白背景上）
	// 我们必须强行将其标记为 Handled()！
	// 这一步不仅吞噬了幽灵点击防走火，更保住了 UI 的输入焦点不被底层视口夺走！
	return Reply.IsEventHandled() ? Reply : FReply::Handled();
}

FReply UMyActivatableWidgetBase::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 1. O(1) 动态查表：看看双击的这个键（比如双击中键）是否在白名单里
	if (IsPassthroughAction(InMouseEvent.GetEffectingButton()))
	{
		return FReply::Unhandled();
	}

	// 2. 让父类处理常规的 UI 逻辑
	FReply Reply = Super::NativeOnMouseButtonDoubleClick(InGeometry, InMouseEvent);

	// 3. 【双击护盾修复】：哪怕是连点，只要不在白名单里，照样强行吞噬！
	return Reply.IsEventHandled() ? Reply : FReply::Handled();
}

FReply UMyActivatableWidgetBase::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// 【动态动作寻址】：向 O(1) 缓存查询当前按下的物理键（无论键盘还是手柄）是否隶属于蓝图配置的免检动作名单。
	if (IsPassthroughAction(InKeyEvent.GetKey()))
	{
		// 动作命中：主动放弃拦截（Unhandled）。
		// 让信号漏回全局增强输入子系统，确保“关闭面板”等全局统筹指令顺利触发，彻底杜绝 UI 霸占输入导致的“操作死锁”。
		return FReply::Unhandled();
	}

	// 动作未命中：交由父类执行原生 UI 路由。
	// 意义：在 UI 焦点层彻底销毁该按键信号（如 WASD 或空格），防止玩家在浏览 UI 期间，底层场景中的主角发生盲目的位移或误放技能。
	FReply Reply = Super::NativeOnKeyDown(InGeometry, InKeyEvent);

	// 【防走位死锁修复】：如果父类不认识这个键（比如不在白名单里的非导航键）
	// 绝对不能放行！强行返回 Handled() 在 UI 层将信号彻底销毁！
	return Reply.IsEventHandled() ? Reply : FReply::Handled();
}

void UMyActivatableWidgetBase::RefreshInputPassthroughCache()
{
	// 1. 肃清旧账：清空当前缓存的物理按键集合
	// @防线：应对“玩家改键”或“输入上下文 (IMC) 移除”的必须操作。
	// 若不强制清空，玩家弃用的旧键位仍会驻留在内存中，导致“幽灵放行”Bug
	CachedPassthroughKeys.Empty();

	// 2. 作用域收束与安全寻址：精准获取当前 UI 的宿主本地玩家
	if (ULocalPlayer* LP = GetOwningLocalPlayer())
	{
		// 3. 获取增强输入子系统：定位掌管该玩家按键管线的底层数据库
		if (auto* InputSubsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			// 4. 【修复核心】：不再拉取全量“可自定义映射”表，而是反向操作！
			// 直接遍历蓝图里配好的“免检动作 (PassthroughActions)”
			for (UInputAction* Action : PassthroughActions)
			{
				if (Action)
				{
					// 5. 暴力查询：向子系统逼问“当前处于激活状态的上下文中，这个 Action 到底绑了哪些物理键？”
					// 此 API 无视任何设置，只要底层能按出这个 Action，它就会把真实硬件键（FKey）交出来！
					TArray<FKey> MappedKeys = InputSubsystem->QueryKeysMappedToAction(Action);

					// 6. 填入 O(1) 缓存表
					for (const FKey& Key : MappedKeys)
					{
						// 7. 缓存建仓：动作命中！将其绑定的【底层物理硬件键位】存入 O(1) 哈希集合
						// @架构闭环：经过这轮提纯后，高频运行的 IsPassthroughAction 函数将直接对硬件 FKey 进行哈希匹配，
						// 彻底剥离了复杂的 Action 溯源逻辑，实现了零开销的极限性能过滤。
						CachedPassthroughKeys.Add(Key);
					}
				}
			}
		}
	}
}

bool UMyActivatableWidgetBase::IsPassthroughAction(const FKey& InKey) const
{
	// 【零堆分配 (Zero Allocation)】：直接读取已有缓存，没有任何临时数组的创建与内存申请，彻底杜绝高频按键引发的内存碎片与 GC 微卡顿。
	// 【O(1) 时间复杂度】：依托 TSet 底层的哈希算法 (Hash)，跳过所有 for 循环遍历，一步算出绝对内存地址进行精准比对。
	// 无论名单多长，查询耗时永远锁定在极速的单次运算。
	// 【布尔决断】：查中则返回 true（引导上层 Unhandled 放行）；未中则返回 false（引导上层 Super 拦截）。
	return CachedPassthroughKeys.Contains(InKey);
}
#pragma endregion
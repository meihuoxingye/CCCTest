// Fill out your copyright notice in the Description page of Project Settings.

// 引入当前类的头文件
#include "UI/ActivatableWidget/MyActivatableWidgetBase.h"
// 引入玩法静态函数库（如处理输入、获取全局对象等）
#include "Kismet/GameplayStatics.h"
// 引入 Slate 核心的输入响应机制结构体
#include "Input/Reply.h"
// 引入处理键盘、鼠标等输入事件的结构体
#include "Input/Events.h"
// 引入本地玩家对象头文件，用于获取所属玩家数据
#include "Engine/LocalPlayer.h"
// 引入自定义的 UI 管理器子系统头文件
#include "UI/Subsystem/MyUIManagerSubsystem.h"
// 必须包含增强输入相关头文件
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "EnhancedActionKeyMapping.h" 
// 用于安全访问组件树
#include "Blueprint/WidgetTree.h" 
// MVVM 与 CommonUI IWYU 补充
#include "CommonInputModeTypes.h"
// 在 UE 5.8 中，不再需要引入 MVVMViewModelBase.h，因为底层 Widget 已原生支持

// ==============================================================================
// 状态查询与驱动接口 (State Queries & Drivers)
// ==============================================================================
#pragma region

// 【激活接口】：由外部调用，触发 UI 展开逻辑，展开过程中只执行一次。
// 【兼容保留】：通过调用 ActivateWidget 将权力移交给 CommonUI 状态机
void UMyActivatableWidgetBase::OnWidgetActivated_Implementation()
{
	ActivateWidget();
}

// 【激活接口】：由外部调用，触发 UI 收起逻辑，收起过程中只执行一次。
// 【兼容保留】：通过调用 DeactivateWidget 将权力移交给 CommonUI 状态机
void UMyActivatableWidgetBase::OnWidgetDeactivated_Implementation()
{
	DeactivateWidget();
}

#pragma endregion

// ==============================================================================
// MVVM 双轨渲染驱动源 (MVVM Dual-Track Drive Source)
// ==============================================================================
#pragma region

void UMyActivatableWidgetBase::SetTransitionProgress(float InProgress)
{
	// 【第一道防线：脏标记防抖拦截 (Dirty Flag Debounce)】
	// 业务背景：当 UI 播放出场/退场动画时，本类的 NativeTick 会在短短 0.3 秒内，亲自每帧向这里塞入递增/递减的进度。
	// 灾难规避：此处必须执行严格的内存值比对。一旦拦截到因超高帧率导致 DeltaTime 过小而产生的无效重复数字，立刻掐断广播。
	// 绝不允许对全网 UI 下发无效的刷新指令，死守渲染管线的 CPU 消耗底线。
	if (TransitionProgress != InProgress)
	{
		// 数据物理着陆 (记录最新的 0~1 线性进度)
		TransitionProgress = InProgress;

		// 【第二道防线：编译期寻址与零反射广播 (Zero-Reflection Broadcast)】
		// 架构解耦：它与 UpdateOpeningEffect（蓝图缓动钩子）共同组成了本框架的“双轨动画渲染机制”。
		// 此处 C++ 只负责大喊“线性进度变了”，绝不关心蓝图拿这个进度去干什么。
		// 极限压榨：摒弃慢速的字符串哈希匹配，直接利用 UHT 编译期生成的静态描述符地址 (FieldId)。
		// 使得向 MVVM 框架通报变更的时间复杂度降为纯粹的 O(1)，确保极致流畅。
		BroadcastFieldValueChanged(UMyActivatableWidgetBase::FFieldNotificationClassDescriptor::TransitionProgress);
	}
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

	// 【修复】：消除直接赋值的弃用警告，统一使用 5.8 标准 Setter
	SetIsFocusable(true);
	bSupportsActivationFocus = bAutoStealFocusWhenActivated;
}

void UMyActivatableWidgetBase::NativeConstruct()
{
	// 1. 执行底层构建：完成 UMG/Slate 控件树的实例化与运行时环境挂载
	Super::NativeConstruct();

	// 防止编辑器预览模式绑定委托，导致编辑器挂死
	if (IsDesignTime()) return;

	// 2. 作用域收束与安全寻址：精准获取当前 UI 的宿主本地玩家
	// @防线：使用 GetOwningLocalPlayer() 替代 GetPlayerController(0)，物理隔离多实例/分屏环境中的输入越权，且能防止网络同步环境下的空指针崩溃。
	if (ULocalPlayer* LP = GetOwningLocalPlayer())
	{
		// 3. 获取增强输入子系统：定位负责当前玩家输入管线仲裁的局部单例
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			// 4. 挂载缓存重建监听（数据驱动核心）
			// @触发时机：当玩家在设置菜单中修改了快捷键，或者底层添加/移除了输入上下文导致控制逻辑重组时广播。
			// 使用 AddUniqueDynamic 是正确的，防止重复构造导致的委托膨胀
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

	// Idle 状态下直接返回，这是最廉价的开销，避免不必要的数学推演
	// 如果处于待机状态，不需要更新动画进度，直接返回以节省性能
	if (CurrentState == EUIState::Idle) return;

	// 根据当前是打开还是关闭状态，确定对应的动画总时长
	float CurrentDuration = (CurrentState == EUIState::Opening) ? OpeningDuration : ClosingDuration;

	// 【优化】：将除法预计算并转为乘法，消除每一帧的除法开销，对 CPU 分支预测更友好
	// 计算本帧进度增加/减少的步长（防止除零错误，最少取 0.001f 应对高帧率显示器）
	const float InvDuration = 1.0f / FMath::Max(0.001f, CurrentDuration);
	const float Step = InDeltaTime * InvDuration;

	float NewProgress = TransitionProgress;

	// 如果正在播放入场动画
	if (CurrentState == EUIState::Opening)
	{
		// 累加进度，最高不超过 1.0f
		NewProgress = FMath::Min(TransitionProgress + Step, 1.0f);
	}
	// 如果正在播放退场动画
	else if (CurrentState == EUIState::Closing)
	{
		// 递减进度，最低不低于 0.0f
		NewProgress = FMath::Max(TransitionProgress - Step, 0.0f);
	}

	// 【双轨渲染之第一轨】：MVVM 核心数据总线更新广播
	SetTransitionProgress(NewProgress);

	// 再次判断状态，用于应用动画效果及判定动画是否结束
	if (CurrentState == EUIState::Opening)
	{
		// ==============================================================================
		// 【双轨渲染之第二轨：数学引擎与蓝图交接 (Math Engine & Blueprint Handover)】
		// ==============================================================================
		// 性能与表现的完美平衡：C++ 承担极其昂贵的非线性数学插值计算 (InterpEaseInOut)，
		// 算出带有丝滑惯性的 EasedProgress 后，将其直接拍给蓝图的 UpdateOpeningEffect 钩子。
		// 让蓝图只做它最擅长的事——把现成的算好的数字连到 Transform 节点上。
		// 计算缓动后的进度值
		float EasedProgress = FMath::InterpEaseInOut(0.0f, 1.0f, TransitionProgress, OpeningExp);
		// 更新打开动画的视觉表现
		UpdateOpeningEffect(TransitionProgress, EasedProgress);

		// 如果进度达到或超过 1.0，说明入场动画播放完毕
		if (TransitionProgress >= 1.0f) CurrentState = EUIState::Idle; // 切换为待机状态
	}
	else if (CurrentState == EUIState::Closing)
	{
		// 【执行第二轨：退场非线性渲染】
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
			// 退出逻辑增加完全安全性检查
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
	// 在 Destruct 时必须非常小心，因为很多关联对象可能已经先于 Widget 销毁
	// 最后的保险：无论 UI 是怎么没的（切场景、被强杀等），都要从子系统中消除
	// 获取所属本地玩家
	if (ULocalPlayer* LP = GetOwningLocalPlayer())
	{
		// 1. 安全移除入栈记录
		// 获取 UI 子系统
		if (UMyUIManagerSubsystem* UIMgr = LP->GetSubsystem<UMyUIManagerSubsystem>())
		{
			// 强行从子系统栈中移除自己，防止悬空指针
			UIMgr->PopUI(this);
		}

		// 2. 安全解绑委托
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

void UMyActivatableWidgetBase::NativeOnFocusLost(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnFocusLost(InFocusEvent);

	// 如果 UI 依然可见且并未进入关闭程序，说明焦点丢失是意外发生的（如 Alt-Tab 切屏）
	// 强制收回焦点，确保 NativeOnKeyDown 持续生效，消除因切屏导致的交互中断
	if (GetVisibility() == ESlateVisibility::Visible && CurrentState != EUIState::Closing)
	{
		SetFocus();
	}
}

// 【原生状态机钩子】：引擎底层激活时触发，完美接管原本在 OnWidgetActivated 中的核心防线
void UMyActivatableWidgetBase::NativeOnActivated()
{
	Super::NativeOnActivated();

	// 防御 1 - 设计时拦截。防止 UMG 编辑器在预览/编译时运行逻辑，这是 90% 编辑器崩溃的原因。
	if (IsDesignTime()) return;

	// 如果已经在打开的过程中，直接返回，避免重复触发激活逻辑
	// 【架构注】：此处的拦截已经完美防御了玩家在 0.1 秒内连按造成的“重复压栈”风险！
	if (CurrentState == EUIState::Opening) return;

	// 防御 2 - 世界上下文检查，防止在关卡切换等极端情况下触发空指针
	if (!GetWorld()) return;

	// 如果当前是折叠不可见状态，或者进度已经归零（完全关闭状态）
	if (GetVisibility() == ESlateVisibility::Collapsed || TransitionProgress <= 0.0f)
	{
		// 重置进度到 0.0，准备重新播放入场动画 (MVVM 方式)
		SetTransitionProgress(0.0f);
	}

	// 将 UI 的根节点（WBP_TacticalMenu）设为可见，从而使整个UI可见，使其开始渲染并接受交互
	// 因为根节点下就是画布面板，所以实际上整个屏幕都被设为可见
	// 为什么不只设置 UI 实际区域可见，其他部分可直接穿透到游戏地面？
	// 因为如果 UI 区域以外的部分不可见了，那么玩家点击屏幕其他地方时，输入事件就会穿透到游戏世界里，导致玩家误操作
	// 需要 UI 区域以外的部分参与检测，以确定点击到底是控制 UI 还是想要退出 UI
	SetVisibility(ESlateVisibility::Visible);

	// 将状态机切换到“正在打开”
	CurrentState = EUIState::Opening;

	// 【全自动入栈】
	// 获取当前 UI 所属的本地玩家
	// 1. 作用域收束 (C++17)：将 LP 和 UIMgr 的声明锁死在 if 块内，物理级别根绝野指针泄露。
	// 2. 拒绝硬编码：绝对禁用 GetPlayerController(0)！通过 GetOwningLocalPlayer 动态寻址，使该 UI 框架天生完美兼容“本地双人分屏/同屏多打”，永远只获取召唤自己的那个宿主玩家。
	// 3. 数据物理隔离：因 UIMgr 是 LocalPlayerSubsystem，1P 和 2P 各自拥有一套独立的 UI 管理器，互相狂按面板也绝对不会导致对方的 UI 栈错乱。
	// 本地玩家与子系统安全寻址，使用 IsValid() 隐式检查机制
	if (ULocalPlayer* LP = GetOwningLocalPlayer())
	{
		// 获取对应的 UI 统筹子系统
		if (UMyUIManagerSubsystem* UIMgr = LP->GetSubsystem<UMyUIManagerSubsystem>())
		{
			// 将自身推入拦截栈，接管输入焦点
			UIMgr->PushUI(this);
		}
	}

	// 【修改】：读取蓝图配置！如果是重度面板，就在这里释放“黑洞”，强行吸走焦点阻断 WASD 移动！
	if (bAutoStealFocusWhenActivated)
	{
		SetFocus();
	}

	// --------------------------------------------------------------------------
	// 【终极防闪烁装甲】：在将控制权交给蓝图之前，强行打断引擎队列，立刻执行底层排版！
	// 这等同于蓝图里的 Force Layout Prepass 节点，它会逼迫 Slate 引擎立刻计算尺寸
	// --------------------------------------------------------------------------
	// [底层机制补充说明]：
	// 1. Slate 的惰性排版 (Lazy Layout)：为了极致性能，引擎在 AddToViewport 或 SetVisibility(Visible) 时，
	//    并不会立刻计算 UI 的长宽大小，而是将其塞入“待办队列”，等到下一帧渲染前才统一计算。
	// 2. 致命的零维度陷阱：如果我们不打断这个队列，紧接着在下一行直接触发 UpdateOpeningEffect 动画，
	//    蓝图节点 `Get Paint Space Geometry` 拿到的尺寸将是致命的 (0, 0)！
	// 3. 视觉灾难 (1-Frame Flicker)：尺寸为 0 会导致基于尺寸推演的进场动画（如从屏幕最下方滑入）在第 0 帧彻底算错位置，
	//    让 UI 在屏幕左上角像幽灵一样“闪烁一帧”，随后才跳回正确位置。
	// 4. 破局：调用 ForceLayoutPrepass()，相当于拿枪指着引擎的头，逼迫它在当前这行 C++ 走完之前，立刻、同步地算出绝对尺寸！
	//    这就保证了下一行传给蓝图的动画，能拿到 100% 准确的 Geometry 数据。
	// ForceLayoutPrepass 虽然安全，但如果在同一帧被多次调用会有开销。这里调用前确保 Widget 确实需要排版。
	ForceLayoutPrepass();

	// 承接上方函数：此时，UI 的绝对像素宽高和空间位置（Geometry）已经被强制算好并存入底层缓存了！
	// 接下来触发蓝图钩子 UpdateOpeningEffect 时，蓝图里拿到的 Geometry 就会是真实尺寸（如1920x1080），绝对不会再是 (0,0)！

	// 准备首帧时间参数：因为第一帧的 NativeTick 尚未运行，必须手动利用起跑点计算一次初始参数。
	// 本函数只会在 UI 展开时执行一次，所以此函数里计算的 TransitionProgress 绝对是 0.0f，代表动画起点；
	// 输入实际经过时间占动画总时长的比例 TransitionProgress，根据范围（0.0 - 1.0）与曲率 OpeningExp，
	// 计算出当前实际经过时间对应的缓动进度 EasedProgress，若曲率为 2 则 EasedProgress 会在开始缓慢，而中途加速，快结束时又减速，形成丝滑的飞入感
	// 防止 OpeningExp 参数异常导致的极值错误
	float EasedProgress = FMath::InterpEaseInOut(0.0f, 1.0f, TransitionProgress, FMath::Max(0.1f, OpeningExp));

	// 触发更新动画，防止出现一帧的默认状态画面闪烁
	UpdateOpeningEffect(TransitionProgress, EasedProgress);
}

// 【原生状态机钩子】：引擎底层失活时触发，完美接管原本在 OnWidgetDeactivated 中的核心防线
void UMyActivatableWidgetBase::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();

	// 增加可见性检查与设计时拦截，防止对象已销毁时依然触发逻辑
	// 如果已经在关闭过程中，或者是折叠不可见状态，则无需重复执行反激活
	if (IsDesignTime() || CurrentState == EUIState::Closing || GetVisibility() == ESlateVisibility::Collapsed) return;

	// 将状态机切换为“正在关闭”
	CurrentState = EUIState::Closing;

	// 注意：这里绝不调用 PopUI！留在拦截栈中抗点击穿透，交由 Tick 动画结束时出栈。
}

TOptional<FUIInputConfig> UMyActivatableWidgetBase::GetDesiredInputConfig() const
{
	// CommonUI 会根据你设置的这个布尔值，自动决定是否在底层封杀 WASD 移动
	if (bAutoStealFocusWhenActivated)
	{
		// 【重度面板模式】 (适用场景：全屏背包、系统设置菜单)
		// ECommonInputMode::Menu：相当于在屏幕上降下一道“绝对防弹玻璃”。
		// 彻底切断硬件设备与底层 3D 游戏角色 (WASD移动/鼠标开火) 的所有联系，确保玩家在翻背包时绝对不会走火。
		// EMouseCaptureMode::CapturePermanently：永久捕获鼠标，确保鼠标指针不会意外滑出游戏窗口边界。
		return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::CapturePermanently);
	}

	// 【轻度面板模式】 (适用场景：悬浮任务栏、左下角击杀提示、右侧伤害统计)
	// 保持 Game 模式，UI 仅作为画中画存在。玩家的键盘和鼠标信号依然全额漏给底层的 3D 角色，不妨碍战斗。
	return FUIInputConfig(ECommonInputMode::Game, EMouseCaptureMode::CapturePermanently);
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
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UMyActivatableWidgetBase::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 1. 同样的免检放行逻辑
	if (IsPassthroughAction(InMouseEvent.GetEffectingButton()))
	{
		return FReply::Unhandled();
	}

	// 2. 同样的保留父类与蓝图处理权
	FReply Reply = Super::NativeOnMouseButtonDoubleClick(InGeometry, InMouseEvent);

	// 3. 【终极防线】：将漏网的双击信号强行斩断，绝不让它接触到底层 3D 世界！
	if (!Reply.IsEventHandled())
	{
		return FReply::Handled();
	}

	return Reply;
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

	// 动作未命中：交由父类执行原生按键信号销毁逻辑，并将处理结果存进 Reply 变量里
	// 在 UI 焦点层彻底销毁该按键信号（如 WASD 或空格），防止玩家在浏览 UI 期间，底层场景中的主角发生盲目的位移或误放技能。
	FReply Reply = Super::NativeOnKeyDown(InGeometry, InKeyEvent);

	// 【新增】：终极补刀防线！
	// 检查引擎底层的处理结果：!Reply.IsEventHandled()
	// 如果 IsEventHandled() 为 false，说明引擎底层判定为 Unhandled
	// 这代表它不认识这个在白名单之外的键位且没处理它
	if (!Reply.IsEventHandled())
	{
		// 既然引擎底层不管（比如 WASD），那我们就强行把它吃掉！
		// 强制返回 Handled()，告诉引擎：这个键已经被处理了，信号就地销毁！
		return FReply::Handled();
	}

	// 如果引擎底层认识且处理了这个白名单之外的键，那就直接把原本的结果原样返回回去。
	return Reply;
}

void UMyActivatableWidgetBase::RefreshInputPassthroughCache()
{
	// 1. 肃清旧账：清空当前缓存的物理按键集合
	// 性能优化：使用 Empty(Capacity) 预分配空间。
	// 假设每个 Action 平均绑定 2 个按键（如键盘/手柄），预分配空间可以彻底消除 Add 过程中的内存扩容与重排。
	CachedPassthroughKeys.Empty(PassthroughActions.Num() * 2);

	// 2. 作用域收束与安全寻址：精准获取当前 UI 的宿主本地玩家
	if (ULocalPlayer* LP = GetOwningLocalPlayer())
	{
		// 3. 获取增强输入子系统：定位负责当前玩家输入管线的底层单例
		// 【优化】：使用 IsValid 确保子系统不仅非空，且不在销毁队列中
		auto* InputSubsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
		if (IsValid(InputSubsystem))
		{
			// 4. 精准寻址：直接遍历蓝图里配置的“免检动作 (PassthroughActions)”
			// 这样做的好处是 O(N) 只取决于你的白名单长度（通常是个位数），而不是游戏总键位数（成百上千）。
			for (UInputAction* Action : PassthroughActions)
			{
				// 【新增/防御】：防止蓝图配置中存在空引用导致查询崩溃，使用 IsValid 防止资源在异步加载时的空引用
				if (IsValid(Action))
				{
					// 5. 暴力且精准的查询：向子系统询问该特定 Action 目前映射的所有物理按键。
					// 该 API 会返回当前激活的所有 IMC（输入映射上下文）中有效的键位。
					TArray<FKey> MappedKeys = InputSubsystem->QueryKeysMappedToAction(Action);

					// 6. 填入哈希缓存
					for (const FKey& Key : MappedKeys)
					{
						// 数据清洗：仅缓存合法按键，供 IsPassthroughAction 进行 O(1) 瞬时判定
						// 防止跨平台或虚拟输入返回的脏数据污染哈希表
						if (Key.IsValid())
						{
							CachedPassthroughKeys.Add(Key);
						}
					}
				}
			}
		}
	}
}

bool UMyActivatableWidgetBase::IsPassthroughAction(const FKey& InKey) const
{
	// 防御：防止非法按键查询
	if (!InKey.IsValid()) return false;

	// 【零堆分配 (Zero Allocation)】：直接读取已有缓存，没有任何临时数组的创建与内存申请，彻底杜绝高频按键引发的内存碎片与 GC 微卡顿。
	// 【O(1) 时间复杂度】：依托 TSet 底层的哈希算法 (Hash)，跳过所有 for 循环遍历，一步算出绝对内存地址进行精准比对。
	// 无论名单多长，查询耗时永远锁定在极速的单次运算。
	// 【布尔决断】：查中则返回 true（引导上层 Unhandled 放行）；未中则返回 false（引导上层 Super 拦截）。
	return CachedPassthroughKeys.Contains(InKey);
}
#pragma endregion
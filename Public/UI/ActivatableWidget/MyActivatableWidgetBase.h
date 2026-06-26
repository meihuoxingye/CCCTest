// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

// 保证头文件只被编译一次，防止重复包含错误
#include "CoreMinimal.h"
// 【替换为 CommonUI】：包含 CommonActivatableWidget 基类
#include "CommonActivatableWidget.h"
// 【必须在最后一行】
// 包含 Unreal Header Tool 生成的反射代码文件
#include "MyActivatableWidgetBase.generated.h"

// IWYU 前置声明
class UInputAction;
struct FKey;

// ==============================================================================
// 事件广播 (Event Broadcasting)
// ==============================================================================

// 声明一个不带参数的动态多播委托，用于在 C++ 广播后让蓝图节点（Event 节点）绑定并响应 UI 的关闭请求。
// 为什么开启请求不需要委托：“开启面板”这个动作的发起者，通常拥有最高级别的权力，
// 有小弟（UI）的直接联系方式（指针），直接调函数（Call Function）是最高效、最直接的，根本不需要委托；
// “关闭”的情况就复杂太多了。触发关闭的源头，往往来自于UI 内部或底层的仲裁系统，
// 而这些修改输入模式、隐藏鼠标的终极权力，UI 面板根本没资格调这些底层 API，只能通过委托发送“请求”
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTacticalMenuCloseRequestedSignature);

// ==============================================================================
// 转换状态机枚举 (Transition State Machine Enum)
// ==============================================================================

// 声明枚举类型，并标记 BlueprintType 使其可以在蓝图系统中作为变量类型被访问
// 使用 uint8 作为底层数据类型以节约内存
UENUM(BlueprintType)
enum class EUIState : uint8
{
	// 使用 UMETA 宏定义在蓝图编辑器中显示的中文本地化名称
	Idle		UMETA(DisplayName = "静止"),
	Opening		UMETA(DisplayName = "展开中"),
	Closing		UMETA(DisplayName = "收起中")
};

/** * 可激活控件基类 (Activatable Widget Base)
 * 3A级战术呼出底层：抗子弹时间、纯数学进度表现驱动、进出场双轨解耦及实时编辑器预览
 */
 // 声明此类为抽象类 (Abstract，无法直接实例化) 并且允许被蓝图继承创建子类 (Blueprintable)
 // 使用时设置其为蓝图基类即可
UCLASS(Abstract, Blueprintable)
// 继承自 UCommonActivatableWidget，带有项目模块的 API 导出宏 (CCC_API)
// 【UE 5.8 特性】：UWidget 基类现已原生集成 INotifyFieldValueChanged，无需手动多重继承！
class CCC_API UMyActivatableWidgetBase : public UCommonActivatableWidget
{
	GENERATED_BODY()

	// ==============================================================================
	// 外部委托接口 (External Delegates)
	// ==============================================================================
public:

	// BlueprintAssignable 允许该委托在蓝图的“事件图表”中被拉出并绑定 (Bind Event)
	// 实例化前面声明的多播委托，用于通知外部（如子系统）该 UI 发起了关闭请求
	UPROPERTY(BlueprintAssignable, Category = "UI|Events")
	FOnTacticalMenuCloseRequestedSignature OnCloseRequested;


	// ==============================================================================
	// 交互配置 (Interaction Configuration)
	// ==============================================================================
public:

	// 控制开关：该面板是否允许玩家通过点击屏幕外（背景）的空白处来关闭它。
	// 如果为 false，点击背景时依然会拦截开火信号，但不会关闭 UI，玩家只能通过显式的“关闭/返回”按钮或快捷键退出。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Interaction")
	bool bCanBeClosedByBackgroundClick = true;

	// 控制开关：该面板在展开时，是否自动抢夺键盘/手柄的输入焦点？
	// 如果为 true：UI 展开瞬间化身“输入黑洞”，玩家的 WASD 会被 NativeOnKeyDown 拦截吞噬，底层角色被定身。适合“重度面板”（如存档、全屏背包）。
	// 如果为 false：UI 处于“礼貌状态”，只拦截鼠标点击，不管键盘输入。玩家可以一边看 UI 一边走路。适合“轻度面板”（如悬浮任务栏）。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Interaction")
	bool bAutoStealFocusWhenActivated = false;


	// ==============================================================================
	// 状态查询与驱动接口 (State Queries & Drivers)
	// ==============================================================================
public:

	// 获取当前 UI 的状态机阶段，供 UIManagerSubsystem 仲裁幽灵点击时调用
	// 声明为纯节点（无执行引脚，BlueprintPure），因为这是一个不会修改类成员状态的 Getter 函数
	// const 后缀保证该函数不会改变类的任何成员变量
	UFUNCTION(BlueprintPure, Category = "UI|Transition")
	EUIState GetCurrentState() const { return CurrentState; }


	// ==============================================================================
	// 核心转换系统配置 (Transition System Settings)
	// ==============================================================================
protected:

	// 核心状态标尺。在 UMG 编辑器细节面板可手动切换状态进行出/退场分流预览。
	// 允许在蓝图编辑器面板的任何位置编辑，并且在蓝图图表中可读可写
	// 默认状态初始化为“静止”
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Transition|State")
	EUIState CurrentState = EUIState::Idle;

	// 出场专属耗时（秒）。通常较长，用于展示张力极强的飞入动画
	// 设置最小限制为 0.01 秒，从源头防止后续 DeltaTime 除零引发崩溃
	// 默认展开动画耗时 0.35 秒
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Transition|Settings", meta = (ClampMin = "0.01"))
	float OpeningDuration = 0.35f;

	// 退场专属耗时（秒）。通常极短（如 0.15），要求干净利落，不拖泥带水
	// 同样限制最小耗时防崩溃
	// 默认收起动画耗时 0.15 秒
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Transition|Settings", meta = (ClampMin = "0.01"))
	float ClosingDuration = 0.15f;

	// 出场缓动指数。推荐 2.0~3.0，飞入时自带丝滑刹车感
	// 限定缓动指数最小为 1.0 (线性)，用于 FMath::InterpEaseInOut 数学函数
	// 默认开场缓动强度为 2.0
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Transition|Settings", meta = (ClampMin = "1.0"))
	float OpeningExp = 2.0f;

	// 退场缓动指数。推荐 1.0 (纯线性)，让透明度匀速消失，绝不拖泥带水
	// 同样限制最少为纯线性
	// 默认退场缓动强度为 1.0
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Transition|Settings", meta = (ClampMin = "1.0"))
	float ClosingExp = 1.0f;


	// ==============================================================================
	// MVVM 双轨渲染驱动源 (MVVM Dual-Track Drive Source)
	// ==============================================================================
protected:

	// 核心时间标尺 (0.0 -> 1.0)，代表当前动画已完成的绝对百分比（现实时间/总时长）。
	// 【底层计算】：由 NativeTick 每帧自动累加或递减。单帧变化步长 = 本帧物理时间差 (DeltaTime) / 动画总耗时 (Duration)。
	// 【架构意义】：将物理秒数强制“归一化”为 0~1 的比例，使后续的缓动曲线公式与蓝图表现彻底脱离具体时长的耦合。
	// 在 UMG 编辑器可拖动滑块实时预览。限制范围在 0.0 到 1.0 之间，避免越界溢出导致后续动画计算错误。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Setter, Getter, Category = "UI|Transition|State", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TransitionProgress = 0.0f;

	// 【双轨渲染之第一轨：MVVM 线性数据总线】
	// 业务场景：控制 UI 菜单出场 (Fade/Slide In) 或退场时的基础视觉表现。
	// 架构真相：本动画不依赖任何外部 Timeline！这是由本类 NativeTick 亲自接管生命周期并进行纯数学步进计算的底层接口。
	// 强制规范：严禁在 C++ 层面写死 SetRenderOpacity() 等表现代码！
	// 工作流：NativeTick 每帧算出最新的 0.0~1.0 线性进度后塞入这里，UI 蓝图通过 MVVM 静态监听此数字，全自动完成基础的视觉演变。
	UFUNCTION(BlueprintCallable, Category = "UI|Transition|State")
	void SetTransitionProgress(float InProgress);

	// 【只读拉取口：UMG 视口绑定的“极速提货通道” (O(1) Data Fetch)】
	// 业务解密：它到底拉取了什么数据？
	// 1. 当本类的 NativeTick 触发了上方 Set 函数并广播“进度变了”后，虚幻 MVVM 框架会自动回调此 Getter 函数。
	// 2. 它拉取的是缓存在内存里的 0.0~1.0 纯净线性浮点数（如 0.5 代表面板目前的物理进度到了 50%）。
	// 3. 引擎拿到这个数字后，会直接塞入 UI 蓝图里绑定的控件透明度或材质参数中。
	// 架构防线：为什么必须是 const 内联函数？
	// 因为在 0.3 秒的自驱动动画里，它会被底层高频“查岗”几十次。const 加内联能提供纯粹 O(1) 的内存直读，严禁在此写任何数学计算，榨干渲染性能！
	float GetTransitionProgress() const { return TransitionProgress; }


	// 【第二轨：入场动画渲染钩子】
	// 【出场专属钩子】：仅在 UI 展开时触发。推荐驱动 Y轴位移 和 透明度渐显。	 
	// @param Progress  线性进度 (0.0 到 1.0)	 * @param EasedProgress  平滑后的视觉进度；
	// 这是一个没有 C++ 实现的事件，强制要求蓝图子类去实现处理展开 UI 时的动画表现。
	// 为什么不在 C++ 写死代码：防止底层被视觉表现绑架！若在 C++ 写死动画逻辑，美术每次修改表现都需要程序员重新编译项目，将千变万化的视觉表演权 100% 移交蓝图
	// 【架构约束/单向通讯】：返回值为 void 宣告了它是“发射后不管”的事件（蓝图红节点）。
	// 【参数规范/下行数据】：括号内的变量是 C++ 在原生层全速算好后“喂”给蓝图的纯只读数据载荷；
	// C++ 仅作为发令枪派发数据，绝不挂起当前线程等待蓝图的执行结果，实现了底层状态机与表层视觉的绝对物理隔离。
	// 【架构定位】：这是本框架 UI 表现力的核心主轴。与第一轨的“线性瞎广播”不同，第二轨是“带数学处理的精准打击”。
	// 【工作流】：本类的 NativeTick 会用高阶多项式（EaseInOut）将死板的线性时间，扭曲成带有“物理惯性（先慢后快再慢）”的 EasedProgress。
	// 然后通过此蓝图事件 (BlueprintImplementableEvent) 直接下发。
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Transition|Animation")
	void UpdateOpeningEffect(float Progress, float EasedProgress);

	// 【第二轨：退场动画渲染钩子】
	// 【退场专属钩子】：仅在 UI 收起时触发。推荐仅驱动透明度渐隐，利用 Slate 特性维持最后坐标。	 
	// @param Progress  线性进度 (0.0 到 1.0)	 * @param EasedProgress  平滑后的视觉进度；
	// 这是一个没有 C++ 实现的事件，强制要求蓝图子类去实现处理收起 UI 时的动画表现。
	// 为什么不在 C++ 写死代码：防止底层被视觉表现绑架！若在 C++ 写死动画逻辑，美术每次修改表现都需要程序员重新编译项目，将千变万化的视觉表演权 100% 移交蓝图
	// 【架构约束/单向通讯】：返回值为 void 宣告了它是“发射后不管”的事件（蓝图红节点）。
	// 【参数规范/下行数据】：括号内的变量是 C++ 在原生层全速算好后“喂”给蓝图的纯只读数据载荷；
	// C++ 仅作为发令枪派发数据，绝不挂起当前线程等待蓝图的执行结果，实现了底层状态机与表层视觉的绝对物理隔离。
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Transition|Animation")
	void UpdateClosingEffect(float Progress, float EasedProgress);


	// ==============================================================================
	// 控件生命周期 (Widget Lifecycle)
	// ==============================================================================
protected:

	/** * 【零点确立】：引擎级单次初始化钩子。
	 * @调用机制：引擎 UMG 框架全自动调用（绝对禁止外部手动调用）。
	 * @调用者：UMG 实例化工厂 (WidgetFactory)。
	 * @调用时机：当执行 CreateWidget 在内存中刚把骨架搭建好时触发，终生仅执行一次。
	 * @职责：在 UI 实例创建时，强行确立状态机的绝对安全起点（重置进度为 0.0，锁定 Idle 状态）。
	 */
	virtual void NativeOnInitialized() override;

	// 【生命周期钩子/拦截网挂载点】：当 UI 底层 Slate 控件树构建完毕，且成功与世界 (World) 及本地玩家 (LocalPlayer) 建立绑定关系时触发。
	// @精准定义：与 NativeOnInitialized（终生仅在内存中执行一次）不同，NativeConstruct 确保了当前 UI 已经具有了完整的运行时上下文。
	// @为何选在此处挂载：在更早的初始化阶段，UI 可能尚未完全依附于所属的玩家，此时去获取增强输入子系统极易触发空指针崩溃。
	// 选在 Construct 阶段，能 100% 安全地获取到宿主玩家的 Subsystem，是建立系统级监听与初始化缓存最严谨的时机。
	// @职责：
	// 1. 绑定增强输入系统的按键映射重构委托（确立“玩家改键自动同步”的数据驱动机制）。
	// 2. 首次主动调用 RefreshInputPassthroughCache()，完成物理按键拦截名单的初始建仓。
	virtual void NativeConstruct() override;

	/** * 【所见即所得】：编辑器与 C++ 数据同步桥梁。
	 * @调用机制：引擎 UMG 框架全自动调用。
	 * @调用者：UMG 编辑器框架 / 运行时实例化工厂。
	 * @调用时机：编辑器内美术拖动参数滑块时实时触发；游戏运行时 UI 创建完毕同步默认参数时触发。
	 * @绝活：允许在不运行游戏的情况下，在 UMG 视口中实时预览缓动曲线计算后的出/退场动画表现。
	 */
	virtual void SynchronizeProperties() override;

	/** * 【核心时钟引擎】：UI 状态机的纯数学驱动源。
	 * @调用机制：引擎主循环全自动调用。
	 * @调用者：虚幻 Tick 管理器 (Tick Manager)。
	 * @调用时机：只要 UI 被 AddToViewport 添加到屏幕且未被暂停，每一帧自动触发。
	 * @防线：精准控制出栈时机——只有当退场动画进度彻底归零（UI 完全消失）后，才通知 UIMgr 解除拦截。
	 */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** * 【终极内存防线】：UI 死亡前的最后遗言。
	 * @调用机制：引擎底层内存管理全自动调用。
	 * @调用者：垃圾回收器 (GC) 或 Slate 销毁队列。
	 * @调用时机：UI 被 RemoveFromParent 且失去所有强引用即将被物理抹除前，或切换关卡导致世界毁灭时触发。
	 * @意义：物理层面根绝一切悬空指针导致的游戏崩溃，强制通知 UIMgr 将自身剔除。
	 */
	virtual void NativeDestruct() override;

	// 【新增】：焦点丢失回调，防止 Alt-Tab 切屏后 UI 彻底失焦卡死
	// 触发时机：在当前 UI 曾经拥有“输入焦点”的前提下，焦点被任何外部力量强行剥夺的瞬间
	virtual void NativeOnFocusLost(const FFocusEvent& InFocusEvent) override;

	/** * 【CommonUI 函数：原生激活挂载点】
	 * @调用时机：UI 被引擎推入激活栈时自动触发（展开过程中仅执行一次）。
	 * @核心职责：将状态切为 Opening，强制底层立刻排版（防闪烁），并将自身推入子系统拦截栈接管焦点。
	 */
	virtual void NativeOnActivated() override;

	/** * 【CommonUI 函数：原生失活挂载点】
	 * @调用时机：UI 被引擎移出激活栈时自动触发（收起过程中仅执行一次）。
	 * @核心职责：将状态切为 Closing ，在 NativeTick 里触发退场动画。注意：为防幽灵点击，此处仅改状态，绝不出栈。
	 */
	virtual void NativeOnDeactivated() override;

	/** * 【CommonUI 函数：输入管线仲裁法则】
	 * @调用时机：UI 激活并成为当前输入焦点时，引擎底层大管家自动调用。
	 * @核心职责：根据当前 UI 配置，向底层返回该面板的输入模式（是否拦截游戏输入）及鼠标捕获边界（是否允许滑出屏幕）。允许子类重写。
	 */
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;


	// ==============================================================================
	// 响应式输入路由 (Input Routing)
	// ==============================================================================
public:

	// 给蓝图暴露一个选项，让你选择用哪个动作来关闭 UI（比如绑了 E 键的 IA_ToggleSaveMenu）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Input Routing")
	TObjectPtr<class UInputAction> CloseUIAction;

protected:

	// 官方的 CommonUI 返回动作回调函数
	void OnBackActionExecuted();

	/** * 【鼠标物理防穿透盾牌】：裁决当前的鼠标点击是“穿透放行给底层 3D 世界”，还是“被 UI 拦截消化（防走火）”。
	 * 活动面板的第一道防线，拦截点击到 UI 的不能放行的鼠标按键，其他可放行的点击 UI 事件以及未点到 UI 的事件将会经过 UI 管理子系统的判决
	 * @调用机制：引擎 UI 框架事件驱动全自动调用。
	 * @调用者：FSlateApplication（虚幻全局 UI 交互大总管）。
	 * @调用时机：玩家按下鼠标，且大总管发射的隐形射线精确命中了当前 UI 的几何体边界时触发。
	 * @策略：向 O(1) 缓存动态查表。命中蓝图配置的免检动作（如允许中键缩放）则精准放行（Unhandled）；未命中则交由父类拦截吞噬（Handled）防止底层主角走火。
	 * @架构说明：为何必须交由父类 (Super) 收尾，而绝不能用任何自定义 UI 检测工具完全替代？
	 * 任何自定义检测函数（即使能成功返回 Handled 拦截信号），都只停留在“业务拦截”层面。
	 * UUserWidget 的父类原生逻辑中，封装着虚幻 Slate 引擎不可替代的核心 UI 状态机更新（包括但不限于：Drag&Drop 拖拽初始化、精准的焦点强夺机制、以及向蓝图图表 OnMouseButtonDown 的事件路由）。
	 * 越过 Super 强行接管，等于直接斩断了当前 UI 与底层引擎状态同步的纽带，会导致一系列引擎级交互失灵。
	 */
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// 【新增】：双击物理防穿透盾牌
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	/** * 【键盘/手柄快捷键断路器】：裁决当前的按键输入是“漏回给全局系统（触发快捷键）”，还是“被 UI 独吞屏蔽（防走位）”。
	 * @调用机制：引擎 UI 框架事件驱动全自动调用。
	 * @调用者：FSlateApplication（虚幻全局 UI 交互大总管）。
	 * @调用时机：玩家按下键盘任意键，且【必须满足当前 UI 已经抢占了引擎的“输入焦点 (User Focus)”】时触发。
	 * @策略：向 O(1) 缓存动态查表。命中蓝图配置的免检动作（如关闭面板的全局快捷键）则主动放行（Unhandled）防死锁；未命中则交由父类彻底吞噬，防止玩家浏览 UI 时底层主角盲目走位。
	 */
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// 允许穿透 UI 拦截的“输入动作 (Input Action)”白名单数组。
	// 在蓝图中将全局动作（如关闭面板、切换地图、移动）添加进来，底层会自动反查对应的真实按键并予以放行，完美兼容玩家自定义改键。
	// “允许穿透”即赋予这些特定按键免检特权，让它们的信号不在 UI 层被销毁，而是漏回给底层的全局系统（如 PlayerController）。
	// 典型应用：必须放行“关闭面板”的全局快捷键（如 Tab、Esc）。如果不允许其穿透，UI 会把关闭指令一起吞噬，导致面板永远关不掉（操作死锁）。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Input Routing")
	TArray<class UInputAction*> PassthroughActions;

	// 【性能优化核心/O(1) 缓存表】：存储当前被允许穿透 UI 的所有底层物理按键（FKey）。
	// @穿透含义详解：当 UI 激活并抢占焦点时，默认会像一堵墙一样“吞噬”所有输入，防止玩家在看面板时底层的 3D 角色还在乱跑或开火。
	// “允许穿透”即赋予这些特定按键免检特权，让它们的信号不在 UI 层被销毁，而是漏回给底层的全局系统（如 PlayerController）。
	// 典型应用：必须放行“关闭面板”的全局快捷键（如 Tab、Esc）。如果不允许其穿透，UI 会把关闭指令一起吞噬，导致面板永远关不掉（操作死锁）。
	// @架构意义：将耗时的“遍历 Action 查按键”逻辑提前转移到初始化与改键阶段。在玩家高频输入时，
	// 仅需对本表进行极速的 O(1) 包含测试（Contains），彻底消除了运行时高频查询产生的 TArray 堆内存分配与 CPU 抖动。
	TSet<FKey> CachedPassthroughKeys;

private:

	// 【缓存重建引擎】：根据蓝图配置的 PassthroughActions，向增强输入系统反查对应的物理按键并重构缓存表。
	// @调用时机：
	// 1. 首次建仓：UI 创建并加入屏幕时（NativeConstruct）。
	// 2. 动态刷新：玩家在系统设置中修改了键位，触发增强输入系统 ControlMappingsRebuiltDelegate 时自动回调。
	// @注意：必须标记为 UFUNCTION()，否则无法被 UE 反射系统识别，进而导致 AddUniqueDynamic 动态多播委托绑定失败。
	UFUNCTION()
	void RefreshInputPassthroughCache();

	// 【高频查询关口】：输入路由的终极仲裁者，判断当前按下的物理键是否拥有穿透 UI 的特权。
	// @参数 InKey：玩家当前按下的真实硬件按键（键盘M、鼠标中键、手柄按键等）。
	// @性能指标：零内存分配（Zero Allocation），绝对 O(1) 时间复杂度，专为极致帧率环境打造。
	// 常引用传递：避免不必要的值复制，保护传入的对象免受修改风险
	// 尾部只读承诺，纯查询函数，调用这个函数绝对不会修改当前 UI 的任何内部状态
	bool IsPassthroughAction(const FKey& InKey) const;

};
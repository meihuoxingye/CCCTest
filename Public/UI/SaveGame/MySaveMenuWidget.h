// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/ActivatableWidget/MyActivatableWidgetBase.h"
#include "MySaveMenuWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;
class UMySaveSlotWidget;

// ==============================================================================
// 存档菜单面板 (Save Menu Widget)
// ==============================================================================
UCLASS()
class CCC_API UMySaveMenuWidget : public UMyActivatableWidgetBase
{
	GENERATED_BODY()

	// ==============================================================================
	// 核心生命周期与初始化 (Core Lifecycle & Initialization)
	// ==============================================================================
public:
	// 构造函数
	UMySaveMenuWidget();

protected:
	// 核心生命周期监听放 NativeOnInitialized，在 Widget 实例的整个生命周期内仅触发一次。
	virtual void NativeOnInitialized() override;

	// UI 蓝图预构建阶段：负责在底层 Slate 控件成型前，处理属性配置（如剥夺按钮焦点）
	virtual void NativePreConstruct() override;

	// 状态依赖监听放 NativeConstruct，每次界面创建后都会触发。
	// UI 的生命周期函数：创建时调用
	virtual void NativeConstruct() override;

	// UI 的生命周期函数：销毁时调用（极其重要，用于解绑委托防止内存泄漏）
	virtual void NativeDestruct() override;


	// ==============================================================================
	// CommonUI 核心重写 (CommonUI Overrides)
	// ==============================================================================
protected:

	// 重写父类 MyActivatableWidgetBase 的函数，父类返回的都是捕获模式
	// 属于 CommonUI 插件，调用时机：
	// 1.面板刚刚被激活变成最顶层时；
	// 2. UI 层级发生变化时（例如：关掉了一个盖在它上面的二级弹窗，焦点重新回到这个面板时，底层会再次调用它来刷新鼠标状态）；
	// 解决鼠标消失：重写输入配置，强制显示鼠标
	virtual TOptional<struct FUIInputConfig> GetDesiredInputConfig() const override;

	// 属于 CommonUI 插件，调用时机：
	// 1.面板刚刚被激活（Activated）时（用于决定第一眼聚焦在哪个卡片/按钮上）；
	// 2.当前焦点意外丢失（例如鼠标点到了屏幕外的空白处），此时玩家突然推动手柄摇杆或按下键盘方向键（底层需要调用此函数来找回初始光标位置）；
	// 解决点击闪退：重写焦点获取目标，打破 Slate 焦点死循环
	virtual class UWidget* NativeGetDesiredFocusTarget() const override;

	// 属于 CommonUI 插件，调用时机：
	// 每次面板成功变为“激活（Active）”状态的那一瞬间触发（每次打开面板都会执行，用来做状态刷新）。
	virtual void NativeOnActivated() override;

	// ==============================================================================
	// 绝对排版与分页系统 (Absolute Layout & Pagination)
	// ==============================================================================
protected:

	// 强制要求蓝图必须有一个叫 Box_SaveSlots 的垂直框，否则 C++ 直接报错拒绝编译
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> Box_SaveSlots;

	// 页码显示 (如 "1 / 3")
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_PageInfo;

	// “上一页”按钮
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UMyCommonButtonBase> Btn_PrevPage;

	// “下一页”按钮
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UMyCommonButtonBase> Btn_NextPage;

	// “加一页”按钮
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UMyCommonButtonBase> Btn_AddPage;

	// 清空本页按钮
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UMyCommonButtonBase> Btn_ClearPage;

	// 整理碎片（删除所有空页）按钮
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UMyCommonButtonBase> Btn_CompactPages;

	// 暴露给蓝图，让美术指定要生成的卡片类 (如 WBP_SaveSlot)
	UPROPERTY(EditDefaultsOnly, Category = "SaveMenu|Classes")
	TSubclassOf<UMySaveSlotWidget> SlotWidgetClass;

	// 暴露给蓝图的边缘导航事件：把首、尾两张卡片传给蓝图，让蓝图自己决定“第一张再往上推”和“最后一张再往下推”时，
	// 光标该跳到哪个外围按钮（如翻页键），避免在 C++ 里把排版写死。
	UFUNCTION(BlueprintImplementableEvent, Category = "SaveMenu|Navigation")
	void BP_SetupBoundaryNavigation(class UMySaveSlotWidget* FirstSlot, class UMySaveSlotWidget* LastSlot);

private:
	// 当前页的页数
	int32 CurrentPage = 1;
	// 缓存每页档位数
	int32 CachedSlotsPerPage = 5;
	// 缓存最高页数上限
	int32 CachedMaxUnlockedPages = 50;

	// 【修改】：双对象池之核心 UI 对象池。
	// 作用：相当于固定在墙上的 5 台“显示器”。在初始化时一次性生成并死死锁住物理内存地址。
	// 彻底消灭玩家疯狂翻页时，反复 CreateWidget 与 RemoveFromParent 带来的 Slate 控件树重建与底层排版的海量算力浪费。
	UPROPERTY()
	TArray<TObjectPtr<UMySaveSlotWidget>> SlotPool;

	// 【新增】：双对象池之核心 MVVM 数据池。
	// 作用：相当于永久连在显示器下的 5 个数据线。
	// 为什么必须用双轨制：在 UE5 架构中 ViewModel 属于 UObject。如果仅复用 UI，翻页时仍需不断 NewObject 创建新底座，会产生大量废弃垃圾引发 GC 瞬时卡顿。
	// 双池架构达成了终极的“零内存分配”：翻页时，大管家只需把底层数据“注入”这 5 个固定的机顶盒。
	UPROPERTY()
	TArray<TObjectPtr<class UMySaveDataObj>> ViewModelPool;

	// 仅在 NativeConstruct（UI 创建时）时调用一次；
	// 1. 蓄满双池：一次性将 5 个 UI 实体与 5 个 MVVM 数据底座实例化并死死锁在内存中，彻底消灭后续翻页时的排版开销与垃圾回收卡顿。
	// 2. 焦点铁壁：利用硬编码为这些“永生不死”的卡片铺设绝对安全的上下导航路线，彻底封死手柄焦点迷失导致的死锁崩溃。
	void InitializeSlotPool();

	// 【核心渲染管线】：将内存镜像中查出的“门牌号与元数据（或空壳）”，极速灌入常驻的 ViewModel 数据池以触发 UI 重绘。
	// 1. 零实例化开销：彻底废除翻页时“销毁重建”卡片的低效做法，全程机械复用永生不死的 UI 物理池与数据池。
	// 2. I/O 斩断：根据当前页码算出物理门牌号，直接去大管家的“常驻内存镜像”中 O(1) 极速哈希查表，绝对避开物理硬盘读取。
	// 3. 双轨制驱动：将底层数据（或空壳）精准灌入 ViewModel 后，发出强制重连信号，彻底封死对象池复用引发的假死脱钩，并全权唤醒蓝图表现层（材质/动画）。
	UFUNCTION()
	void BuildSaveSlotList();

	// 刷新分页按钮“上一页”、“下一页”和“加一页”的状态
	void RefreshPaginationUI(int32 TotalPages);

	// 必须加上 UFUNCTION() 标记，否则 UMG 蓝图里的按钮无法将其绑定到 OnClicked 事件上。
	// 执行“向左翻页”指令：将当前页码减 1，并驱动底层数据池拿着新算出的索引，向这 5 张物理卡片重新注水。
	UFUNCTION()
	void OnPrevPageClicked();

	// 必须加上 UFUNCTION() 标记，暴露给蓝图系统作为委托回调的接收端。
	// 执行“向右翻页”指令：将当前页码加 1，并驱动底层数据池拿着新算出的索引，向这 5 张物理卡片重新注水。
	UFUNCTION()
	void OnNextPageClicked();

	// 必须加上 UFUNCTION() 标记，挂载于 UI 上的“解锁新页/购买扩展槽”按钮。
	// 执行“扩充容量”指令：让大管家在内存台账中把总页数上限 +1，并强制将玩家的画面视角跳转至这全新的一页。
	UFUNCTION()
	void OnAddPageClicked();

	// 必须加上 UFUNCTION() 标记，挂载于 UI 上的“清空本页”危险操作按钮。
	// 执行“批量销毁”指令：一键抹除当前屏幕上这 5 个卡槽对应的所有底层物理存档，并将这 5 个机顶盒瞬间洗白为空壳状态。
	UFUNCTION()
	void OnClearPageClicked();

	// 必须加上 UFUNCTION() 标记，挂载于 UI 上的“碎片整理”高级操作按钮。
	// 执行“磁盘碎片整理”指令：将跨页散落的有效存档强行往前挤压对齐，彻底消除中间夹杂的空档位，优化玩家的存储结构。
	UFUNCTION()
	void OnCompactPagesClicked();

	// ==============================================================================
	// 全局状态监听 (Global State)
	// ==============================================================================
protected:

	// 接收大管家（SaveSubsystem）存盘完成后的广播回调
	UFUNCTION()
	void HandleSaveFinished(bool bSuccess);

	// 预留给 UI 蓝图实现的硬盘数据同步完成时的动画接口。C++ 只负责逻辑，播放流光特效或动画的脏活累活交由蓝图去连节点；
	// 父级的 Opening/Closing：控制的是面板的“生存状态”；子级的 SaveSuccess：控制的是具体的“业务状态”。
	UFUNCTION(BlueprintImplementableEvent, Category = "SaveMenu|Animation")
	void BP_PlaySaveSuccessAnimation();
};
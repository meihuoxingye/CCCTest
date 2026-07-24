// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

// 确保该头文件在整个编译过程中只被包含一次，防止重复定义
#include "CoreMinimal.h"
// 引入 UMG UI 系统中所有用户自定义控件的基类
#include "Blueprint/UserWidget.h"
// 引入自定义的存档游戏类，以便识别 FSaveSlotMetaData 等数据结构
#include "SaveGame/MySaveContainer.h" 
// 虚幻引擎反射系统自动生成的头文件，必须放在最末尾
#include "MySaveSlotWidget.generated.h"

// 前向声明，符合 IWYU 规范，加快编译速度
class UButton;
class UMySaveDataObj;

// ==============================================================================
// 存档槽位界面 (Save Slot Widget)
// ==============================================================================

// Abstract：标记为抽象类禁止直接实例化；Blueprintable：允许在编辑器中基于此类创建蓝图
UCLASS(Abstract, Blueprintable)
class CCC_API UMySaveSlotWidget : public UUserWidget
{
	// 注入虚幻引擎反射系统的核心宏，自动生成底层的样板代码
	GENERATED_BODY()

	// ==============================================================================
	// 生命周期与核心虚函数 (Lifecycle & Core Virtual Functions)
	// ==============================================================================
protected:
	// 核心生命周期监听放 NativeOnInitialized，在 Widget 实例的整个生命周期内仅触发一次。
	virtual void NativeOnInitialized() override;

	// UI 蓝图预构建阶段：负责在底层 Slate 控件成型前，处理属性配置（如剥夺按钮焦点）
	virtual void NativePreConstruct() override;

	// 在 UI 控件被实际创建并添加到屏幕的那一刻触发,每次界面创建后都会触发。
	virtual void NativeConstruct() override;

	virtual void NativeDestruct() override;


	// ==============================================================================
	// MVVM 视图模型绑定 (ViewModel Binding)
	// ==============================================================================
public:

	// 供 UMG View Binding 面板绑定的核心数据源头！
	UPROPERTY(BlueprintReadOnly, FieldNotify, Setter, Getter, Category = "SaveSystem|ViewModel")
	UMySaveDataObj* SlotViewModel;

	// 核心逻辑：动态更新当前 UI 实例的新的 ViewModel 引用，给这张 UI 卡片换上新的存档数据（比如翻页时，把卡片从“存盘1”刷新成“存盘6”，或变成“空档位”）；
	// 并触发底层 FieldNotification 广播，UI 面板（View Bindings）监听到数据变化后，会自动完成所有文字修改和按钮显隐，完全无需蓝图连线；
	// 【注意】：如果 UI 卡片的 ViewModel 引用未改变就不用担心数据显示更新，机顶盒的内存地址没变，MVVM UI 就会自己一直监听着它；
	// 调用时机 1：UMySaveMenuWidget::InitializeSlotPool()，第一次打开这个存档菜单时；
	// 调用时机 2：UMySaveMenuWidget::BuildSaveSlotList()，每一次点击“上一页/下一页”、点击“整理碎片”，或者大管家在后台删完档发出广播时；
	// 架构演进：已废除原有的蓝图接口及事件图表连线机制。控件的状态转换（如按钮可见性切换）现已全权由 UMG View Bindings 框架静默处理。
	// 设计规范：遵循表现与逻辑绝对解耦原则，C++ 端仅维护数据上下文，严禁直接操作视觉表现层 API。
	UFUNCTION(BlueprintCallable, Category = "SaveSystem|ViewModel")
	void SetSlotViewModel(UMySaveDataObj* InViewModel);

	UMySaveDataObj* GetSlotViewModel() const { return SlotViewModel; }


	// ==============================================================================
	// 控件绑定与交互逻辑 (Widget Binding & Interaction Logic)
	// ==============================================================================
public:

	// 【新增】：提供一个绝对可靠的焦点锚点，打破死循环
	// 代码里返回 Btn_Save 是为了确保手柄玩家在点开菜单的一瞬间，有一个合法的、能发光的、能按下去的按钮被自动选中。
	class UMyCommonButtonBase* GetFocusAnchor() const { return Btn_Save; }

protected:

	// 核心宏：强制要求继承此 C++ 类的 UI 蓝图中，必须存在一个同名且同类型的控件，否则编译报错
	// 使用虚幻 5 最新的对象指针 TObjectPtr 替代裸指针，声明“保存”按钮控件
	// 【核心修复】：必须添加 BlueprintReadOnly，授予 MVVM 运行时访问该控件指针的权限！
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<class UMyCommonButtonBase> Btn_Save;

	// 核心宏：强制蓝图绑定同名控件
	// 声明“读取”按钮控件
	// 【核心修复】：必须添加 BlueprintReadOnly，授予 MVVM 运行时访问该控件指针的权限！
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<class UMyCommonButtonBase> Btn_Load;

	// 核心宏：强制蓝图绑定同名控件
	// 声明“删除”按钮控件
	// 【核心修复】：必须添加 BlueprintReadOnly，授予 MVVM 运行时访问该控件指针的权限！
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<class UMyCommonButtonBase> Btn_DeleteSlot;

	// 必须加上 UFUNCTION() 宏，这样此函数才能被注册到虚幻反射系统中，进而绑定到动态多播委托上
	// 声明保存按钮的点击事件响应函数
	UFUNCTION()
	void OnSaveButtonClicked();

	// 注册到反射系统，用于委托绑定
	// 声明读取按钮的点击事件响应函数
	UFUNCTION()
	void OnLoadButtonClicked();

	// 注册到反射系统，用于委托绑定
	// 声明删除按钮的点击事件响应函数
	UFUNCTION()
	void OnDeleteSlotClicked();
};
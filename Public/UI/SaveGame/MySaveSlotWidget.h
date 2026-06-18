// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

// 确保该头文件在整个编译过程中只被包含一次，防止重复定义
#include "CoreMinimal.h"
// 引入 UMG UI 系统中所有用户自定义控件的基类
#include "Blueprint/UserWidget.h"
// 引入自定义的存档游戏类，以便识别 FSaveSlotMetaData 等数据结构
#include "SaveGame/MySaveGame.h" 
// 虚幻引擎反射系统自动生成的头文件，必须放在最末尾
#include "MySaveSlotWidget.generated.h"

// 前向声明，符合 IWYU 规范，加快编译速度
class UButton;

// ==============================================================================
// 存档槽位界面 (Save Slot Widget)
// ==============================================================================

// Abstract：标记为抽象类禁止直接实例化；Blueprintable：允许在编辑器中基于此类创建蓝图
UCLASS(Blueprintable)
class CCC_API UMySaveSlotWidget : public UUserWidget
{
	// 注入虚幻引擎反射系统的核心宏，自动生成底层的样板代码
	GENERATED_BODY()

	// ==============================================================================
	// 生命周期与核心虚函数 (Lifecycle & Core Virtual Functions)
	// ==============================================================================
protected:
	// UI 蓝图预构建阶段：负责在底层 Slate 控件成型前，处理属性配置（如剥夺按钮焦点）
	virtual void NativePreConstruct() override;

	// 重写父类的构造回调，在 UI 控件被实际创建并添加到屏幕的那一刻触发
	virtual void NativeConstruct() override;

public:
	// 极致暴力的注水接口，替代 ListView 繁杂的虚拟化回掉
	void InitSlotData(const FString& InSlotName, const FSaveSlotMetaData* MetaData);

	// ==============================================================================
	// 控件绑定与交互逻辑 (Widget Binding & Interaction Logic)
	// ==============================================================================
protected:

	// 核心宏：强制要求继承此 C++ 类的 UI 蓝图中，必须存在一个同名且同类型的控件，否则编译报错
	// 使用虚幻 5 最新的对象指针 TObjectPtr 替代裸指针，声明“保存”按钮控件
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Save;

	// 核心宏：强制蓝图绑定同名控件
	// 声明“读取”按钮控件
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Load;

	// 核心宏：强制蓝图绑定同名控件
	// 声明“删除”按钮控件
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_DeleteSlot;

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

	// 声明一个没有 C++ 实现的接口，强制交由 UI 蓝图去连节点实现（纯表现层拆分）
	// 蓝图事件：把元数据传给蓝图，bHasData 控制是显示“新建存档”还是显示读取/删除按钮
	UFUNCTION(BlueprintImplementableEvent, Category = "SaveSystem|Slot")
	void BP_OnSlotDataInitialized(const FSaveSlotMetaData& SlotMetaData, bool bHasData);

	// ==============================================================================
	// 内部数据缓存 (Internal Data Cache)
	// ==============================================================================
private:

	// 缓存当前卡片绑定的物理存档槽位名称（例如 "SaveSlot_001"），UI 通过这串字符串作为主键向底层发号施令
	FString CachedSlotName;
};
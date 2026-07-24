// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "UObject/SoftObjectPtr.h"
// 不能用 class 的情况： 当你告诉 UHT “请帮我生成一个访问这个属性的函数包装器”时
// 必须包含，确保 TSoftObjectPtr<UTexture2D> 不需要 class 关键字
#include "Engine/Texture2D.h" 

#include "MyCharacterViewModel.generated.h"

/**
 * 角色状态低频业务视图模型
 * 专门处理和储存诸如头像切换、血量上限变更、选中高亮等发生频率极低（触发式）的状态数据

 * 现代数据驱动 UI（MVVM）不再使用传指针这种强耦合 + Tick 的方式
 * 而用监听网络把把系统变成了“电台广播（ViewModel）”和“收音机（UI 蓝图）”的关系
 */


// Blueprintable 能在新建蓝图时作为父类被继承
// BlueprintType 能在蓝图变量面板里把类型设为此类，使 UMG 资产能识别此类
// 为什么其他 C++ 类不用，因为是继承，父类有在 UCLASS() 里声明了 Blueprintable 或 BlueprintType
UCLASS(BlueprintType)
class CCC_API UMyCharacterViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	// --- 属性定义 (5.7 核心规范：类型字符串必须与函数签名完全一致) ---

	// 实际存储当前生命值的底层数据字段
	// BlueprintReadOnly: 在蓝图节点中只读
	// FieldNotify: 核心宏，标记此变量被纳入 MVVM 监听网络
	// Setter/Getter: 绑定具体的读写函数。警告虚幻引擎的反射系统（尤其是蓝图），想读写必须通过 Setter/Getter
	// 只要这个变量的数值通过 Setter 函数发生变化，引擎底层就会发出一个事件通知，调用 Getter 和原生 UI 函数修改 UI 显示
	UPROPERTY(BlueprintReadOnly, FieldNotify, Setter = SetHealth, Getter = GetHealth, Category = "MVVM")
	float Health = 100.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Setter = SetMaxHealth, Getter = GetMaxHealth, Category = "MVVM")
	float MaxHealth = 100.f;

	// 同上，将头像软引用标记为 MVVM 字段。UI 绑定后，配合你写的转换器函数进行加载与渲染
	// 实际存储头像图片硬盘路径的软引用指针
	// 软引用的本质，不是图片本身，而是写着图片硬盘地址的字符串路径
	UPROPERTY(BlueprintReadOnly, FieldNotify, Setter = SetCharacterAvatar, Getter = GetCharacterAvatar, Category = "MVVM")
	TSoftObjectPtr<UTexture2D> CharacterAvatar;

	// 同上，标记选中状态为可广播的 MVVM 字段。可用于控制 UI 上“高亮框”的显示与隐藏
	// 实际存储是否被选中的布尔标识，默认未选中
	UPROPERTY(BlueprintReadOnly, FieldNotify, Setter = SetIsSelected, Getter = IsSelected, Category = "MVVM")
	bool bIsSelected = false;


	// ==============================================================================
	// MVVM 框架绑定的属性访问器 (MVVM Field Accessors)
	// ==============================================================================
public:

	// 写入当前生命值。
	// （内部自带防重绘优化：只有新旧数值不同时，才会执行实际赋值并触发 UI 刷新广播）
	UFUNCTION(BlueprintCallable, Category = "MVVM")
	void SetHealth(float InValue);

	// 读取当前生命值。
	// （作为 MVVM 视图绑定管线拉取最新数据的标准出口）
	UFUNCTION(BlueprintPure, Category = "MVVM")
	float GetHealth() const;

	// 写入最大生命值。
	// （仅在数值发生实质改变时，更新数据并触发广播）
	UFUNCTION(BlueprintCallable, Category = "MVVM")
	void SetMaxHealth(float InValue);

	// 读取最大生命值。
	UFUNCTION(BlueprintPure, Category = "MVVM")
	float GetMaxHealth() const;

	// 写入头像资产路径。
	// （仅在路径发生实质改变时，更新数据并触发广播）
	UFUNCTION(BlueprintCallable, Category = "MVVM")
	void SetCharacterAvatar(TSoftObjectPtr<UTexture2D> InValue);

	// 读取头像资产路径。
	UFUNCTION(BlueprintPure, Category = "MVVM")
	TSoftObjectPtr<UTexture2D> GetCharacterAvatar() const;

	// 写入选中状态。
	// （仅在状态发生翻转时，更新布尔值并触发广播）
	UFUNCTION(BlueprintCallable, Category = "MVVM")
	void SetIsSelected(bool bInSelected);

	// 读取选中状态。
	UFUNCTION(BlueprintPure, Category = "MVVM")
	bool IsSelected() const;
};
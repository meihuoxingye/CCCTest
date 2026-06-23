// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h" 
#include "SaveGame/MySaveContainer.h" 
#include "MySaveDataObj.generated.h"


/**
 * 【存档系统 MVVM 核心视图模型 (Save System MVVM Core ViewModel)】
 * @架构定位：连接底层存档管家与前端 UI 渲染管线的“智能数据中枢（电台）”。
 * @核心职责：
 * 1. 响应式拦截：内建脏数据防抖（Dirty Check），拦截无效的重复赋值，绝对保护 UI 的渲染性能。
 * 2. 局部精准广播：基于 UHT 编译期生成的 FieldId，向外发射 O(1) 级别的变更信号，驱动 UI 实现局部精准重绘，彻底消灭 Tick。
 * 3. 双轨隔离：完美阻断 C++ 底层物理逻辑与 UMG 蓝图视觉表现的直接耦合，是“数据驱动表现”的终极纽带。
 */
UCLASS(BlueprintType)
class CCC_API UMySaveDataObj : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	// 【架构解密】：为什么在 UMG 编辑器的 Viewmodels“绑定 (Bind)”面板里，能看到这三个变量？
	// 想要让一个 C++ 变量出现在 UI 面板的绑定列表中，必须同时满足底层引擎的“三方会签”：
	// 1. 【准入通行证 (FieldNotify)】：最核心的宏！它会指示 UHT (虚幻头文件生成工具) 在编译期为该变量生成专属的静态描述符 (FieldId)。
	// 没有它，变量就只是死数据；有了它，变量才会被注册为一个可以被 UI 控件监听的“活体广播频道”。
	// 2. 【数据暴露权 (BlueprintReadOnly)】：赋予蓝图虚拟机跨界读取 C++ 内存的权限。如果不加这个，UMG 视口连这个变量的影子都摸不到。
	// 3. 【读写规范阀 (Setter/Getter)】：警告引擎底层的 MVVM 绑定管线：绝对不允许直接修改内存！
	// 所有的读写操作必须强行通过指定的访问器函数，从而确保每一次数值变化，都能 100% 触发底层的 Broadcast 广播通知全网刷新。

	// 携带从子系统内存镜像中取出的元数据
	UPROPERTY(BlueprintReadOnly, FieldNotify, Setter, Category = "SaveData")
	FSaveSlotMetaData MetaData;

	// 缓存主键，利用 FieldNotify 支持局部精准刷新
	UPROPERTY(BlueprintReadOnly, FieldNotify, Setter, Getter, Category = "SaveData")
	FString SlotName;

	// 是否为空档的布尔标示
	UPROPERTY(BlueprintReadOnly, FieldNotify, Setter = "SetIsEmptySlot", Getter = "GetIsEmptySlot", Category = "SaveData")
	bool bIsEmptySlot;


public:
	// UMySaveMenuWidget::BuildSaveSlotList() 调用
	// 【核心修复 1】：利用 UHT 生成的静态常量进行 O(1) 广播，绝不走字符串查表！
	// @架构解密：这是 MVVM 底层最硬核的手动广播方式。
	// 对于复杂的自定义结构体 (FSaveSlotMetaData)，如果没写 operator==（判等重载），
	// 使用引擎宏可能会导致编译报错。因此我们选择手动赋值，并直接向 UHT 编译期生成的 
	// FFieldNotificationClassDescriptor 静态地址开火。
	// 时间复杂度绝对 O(1)，彻底规避了传统反射系统用 FName 字符串比对带来的 CPU 哈希开销！
	void SetMetaData(const FSaveSlotMetaData& InMetaData)
	{
		MetaData = InMetaData;
		BroadcastFieldValueChanged(UMySaveDataObj::FFieldNotificationClassDescriptor::MetaData);
	}

	// MVVM 数据拉取口。蓝图 UI 收到上方广播后，会自动调用此 Getter 拿走最新结构体用于渲染。
	FSaveSlotMetaData GetMetaData() const { return MetaData; }


	// UMySaveMenuWidget::BuildSaveSlotList() 调用
	// 【工业级赋值宏】：原生 MVVM 状态刷新标准范式
	// @底层解密：不要被宏魔法骗了！UE_MVVM_SET_PROPERTY_VALUE 在底层默默帮你干了三件极其严谨的事：
	// 1. 【脏数据防抖 (Dirty Check)】：它会先比较 InName 和当前的 SlotName 是否一模一样。如果一样，直接 return 掐断执行！绝对不允许向全网 UI 下发无效的刷新指令。
	// 2. 【安全赋值】：如果不一样，才会执行 SlotName = InName。
	// 3. 【零反射广播】：最后，它在内部自动调用了和你上面手写的一模一样的 BroadcastFieldValueChanged(FieldId) 代码！
	void SetSlotName(const FString& InName)
	{
		UE_MVVM_SET_PROPERTY_VALUE(SlotName, InName);
	}

	// MVVM 数据拉取口。蓝图 UI 收到上方广播后，会自动调用此 Getter 拿走最新结构体用于渲染。
	FString GetSlotName() const { return SlotName; }


	// UMySaveMenuWidget::BuildSaveSlotList() 调用
	// 【空槽位状态翻转锁】
	// 同上，利用引擎原生的 MVVM 赋值宏，精准处理基础数据类型 (bool) 的防抖与广播。
	// @业务场景：当玩家在一个“空槽位”上成功写入数据，底层将其翻转为 false 时，
	// 这行代码会瞬间向上传导。UI 蓝图里绑定的“新建存档”文字会自动变为“存档一”，同时加号图标被隐藏，
	// 全程不需要在 UI 里写任何一根执行白线（Event 节点）！这就是数据驱动的魅力。
	void SetIsEmptySlot(bool bIsEmpty)
	{
		UE_MVVM_SET_PROPERTY_VALUE(bIsEmptySlot, bIsEmpty);
	}

	// MVVM 数据拉取口。蓝图 UI 收到上方广播后，会自动调用此 Getter 拿走最新结构体用于渲染。
	bool GetIsEmptySlot() const { return bIsEmptySlot; }
};
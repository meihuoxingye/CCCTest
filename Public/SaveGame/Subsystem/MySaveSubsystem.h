// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SaveGame/MySaveGame.h"
#include "MySaveSubsystem.generated.h"

// ==============================================================================
// 委托声明 (Delegates Declaration)
// ==============================================================================

// 蓝图可用的多播委托：用于异步存档写盘结束时，通知 UI 弹出“保存成功”或“保存失败”提示
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSaveFinishedSignature, bool, bSuccess);

// 蓝图可用的多播委托：当存档列表发生增删改（比如删档、存新档）时，通知 UI 刷新列表
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSaveRegistryChangedSignature);

// C++ 专属多播大喇叭（架构精髓）：
// 存档瞬间向全图广播，把新建的 SaveObj 传出去。
// 让背包系统、任务系统自己把数据“塞”进 SaveObj 里，实现 SaveSubsystem 与业务逻辑的绝对解耦！
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGameSavingSignature, UMySaveGame* /*SaveObj*/);

// C++ 专属多播大喇叭（读档钩子）：
// 读档成功后，把装满数据的 SaveObj 传给全图。各系统自己把数据“拿”出来恢复状态。
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGameLoadingSignature, UMySaveGame* /*SaveObj*/);

// ==============================================================================
// 全局存档统筹子系统 (Global Save Subsystem)
// ==============================================================================

// 继承自 UGameInstanceSubsystem：它的生命周期与游戏进程同寿，切换关卡绝对不会被销毁
UCLASS()
class CCC_API UMySaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

	// ==============================================================================
	// 核心接口 (Core Interfaces)
	// ==============================================================================
public:

	// 对外暴露的事件绑定点：存盘结果
	UPROPERTY(BlueprintAssignable, Category = "SaveSystem|Events")
	FOnSaveFinishedSignature OnSaveFinished;

	// 对外暴露的事件绑定点：当底层的 CachedRegistry 内存实例成功加载（从硬盘读入或新建兜底成功），或内部存档数据发生增删时广播
	// 委托回调函数 UMySaveMenuWidget::BuildSaveSlotList() 用来刷新 UI
	UPROPERTY(BlueprintAssignable, Category = "SaveSystem|Events")
	FOnSaveRegistryChangedSignature OnSaveRegistryChanged;

	// C++ 内部总线：写入数据钩子
	FOnGameSavingSignature OnGameSaving;

	// C++ 内部总线：读取数据钩子
	FOnGameLoadingSignature OnGameLoading;

	// 在物理硬盘上后台异步识别并提取名为“GlobalSaveRegistry”的存档文件；
	// 注：存档文件只有名称、时间、关卡标识的数据，不包含具体游戏状态数据（如玩家位置、血量等）。
	// 并且绑定识别完毕时的单播委托回调函数 OnRegistryLoaded，将在回调函数把物理文件转为游戏数据
	// UI 处理组件的 BeginPlay 中调用（等玩家出生 2 秒后，后台再开始调用）
	void PreloadRegistry();

	// 核心功能：非阻塞式异步存盘。传入档位名字即可
	UFUNCTION(BlueprintCallable, Category = "SaveSystem|Execution")
	void PerformAsyncSave(const FString& SlotName);

	// 核心功能：删除指定档位，并同步清理注册表
	UFUNCTION(BlueprintCallable, Category = "SaveSystem|Execution")
	bool DeleteSaveSlot(const FString& SlotName);

	// 核心功能：同步读档。读档通常需要立刻重置世界状态，所以暂时用同步处理
	UFUNCTION(BlueprintCallable, Category = "SaveSystem|Execution")
	bool LoadGameFromSlot(const FString& SlotName);

	// O(1) 极速读内存。直接返回内存中缓存的档位列表，消除磁盘 IO 卡顿
	// UMySaveMenuWidget::BuildSaveSlotList() 构建列表时调用
	UFUNCTION(BlueprintCallable, Category = "SaveSystem|Query")
	TArray<FSaveSlotMetaData> GetSaveSlotList();

	// ==============================================================================
	// 内部数据缓存 (Internal Data Cache)
	// ==============================================================================
private:

	// 缓存的全局存档常驻内存目录表，本质上是继承自 USaveGame 的数据容器
	// [数据功能] 仅存储所有槽位的索引元数据（名称、时间、关卡标识），不包含具体游戏状态数据（如玩家位置、血量等）。
	// [生命周期] 通过 UPROPERTY 强引用标记，强制拦截垃圾回收(GC)巡检，防止此目录表被误销毁。
	UPROPERTY()
	TObjectPtr<UMySaveRegistry> CachedRegistry;

	// ==============================================================================
	// 内部管线 (Internal Pipeline)
	// ==============================================================================
private:

	// 异步加载回调函数，当在硬盘上读取到存档文件内存后自动触发
	// 将物理存档文件转换为的对象储存到 CachedRegistry 存档常驻内存目录表里
	// 并且只有在储存成功的情况下，才在 OnSaveRegistryChanged 发送广播通知 UI 刷新列表
	void OnRegistryLoaded(const FString& SlotName, const int32 UserIndex, USaveGame* LoadedGame);

	// 内部工具：每次存盘时，更新内存镜像，并把最新的“目录”写进物理硬盘
	void UpdateSaveRegistry(const FString& SlotName, FName CurrentLevelName);

	// 内部回调：底层 AsyncSaveGameToSlot 执行完毕后触发，用于向 UI 宣告结果
	// AsyncSaveGameToSlot 异步保存，当后台线程保存结束时会调用这个回调函数
	void OnAsyncSaveComplete(const FString& SlotName, const int32 UserIndex, bool bSuccess);
};
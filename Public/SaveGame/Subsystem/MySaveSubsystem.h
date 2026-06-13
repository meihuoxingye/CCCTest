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

	// 对外暴露的事件绑定点：注册表变化（UI 刷新用）
	UPROPERTY(BlueprintAssignable, Category = "SaveSystem|Events")
	FOnSaveRegistryChangedSignature OnSaveRegistryChanged;

	// C++ 内部总线：写入数据钩子
	FOnGameSavingSignature OnGameSaving;

	// C++ 内部总线：读取数据钩子
	FOnGameLoadingSignature OnGameLoading;

	// 异步加载注册表入口。在游戏刚启动或 UI 预热时调用，防止玩家点开存档面板时画面卡顿
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

	// 改为 O(1) 极速读内存。直接返回内存中缓存的档位列表，消除磁盘 IO 卡顿
	UFUNCTION(BlueprintCallable, Category = "SaveSystem|Query")
	TArray<FSaveSlotMetaData> GetSaveSlotList();

	// ==============================================================================
	// 内部数据缓存 (Internal Data Cache)
	// ==============================================================================
private:

	// 存档注册表的常驻内存镜像
	// 只保存每个档位的“元数据”(名字、时间、关卡名)，不存具体血量位置，内存占用极小。
	UPROPERTY()
	TObjectPtr<UMySaveRegistry> CachedRegistry;

	// ==============================================================================
	// 内部管线 (Internal Pipeline)
	// ==============================================================================
private:

	// 异步加载回调函数，当硬盘上的注册表读取到内存后自动触发
	void OnRegistryLoaded(const FString& SlotName, const int32 UserIndex, USaveGame* LoadedGame);

	// 内部工具：每次存盘时，更新内存镜像，并把最新的“目录”写进物理硬盘
	void UpdateSaveRegistry(const FString& SlotName, FName CurrentLevelName);

	// 内部回调：底层 AsyncSaveGameToSlot 执行完毕后触发，用于向 UI 宣告结果
	// AsyncSaveGameToSlot 异步保存，当后台线程保存结束时会调用这个回调函数
	void OnAsyncSaveComplete(const FString& SlotName, const int32 UserIndex, bool bSuccess);
};
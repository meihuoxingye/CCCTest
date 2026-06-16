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

// 蓝图可用的多播委托：当存档列表发生增删改（比如删档、扩容页数）时，通知 UI 刷新列表
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

	// 对外暴露的事件绑定点：当底层的 CachedRegistry 内存实例成功加载，或内部发生增删时广播
	UPROPERTY(BlueprintAssignable, Category = "SaveSystem|Events")
	FOnSaveRegistryChangedSignature OnSaveRegistryChanged;

	// C++ 内部总线：写入数据钩子
	FOnGameSavingSignature OnGameSaving;

	// C++ 内部总线：读取数据钩子
	FOnGameLoadingSignature OnGameLoading;

	// 在物理硬盘上后台异步识别并提取名为“GlobalSaveRegistry”的存档文件；
	// AMyGameModeBase::StartPlay() 游戏开始 2 秒后调用
	void PreloadRegistry();

	// 核心功能：非阻塞式异步存盘。传入档位名字即可；
	// UMySaveSlotWidget::OnSaveButtonClicked() 按下保存键调用
	UFUNCTION(BlueprintCallable, Category = "SaveSystem|Execution")
	void PerformAsyncSave(const FString& SlotName);

	// 核心功能：删除指定档位，并同步清理注册表
	UFUNCTION(BlueprintCallable, Category = "SaveSystem|Execution")
	bool DeleteSaveSlot(const FString& SlotName);

	// 核心功能：同步读档。读档通常需要立刻重置世界状态，所以暂时用同步处理
	UFUNCTION(BlueprintCallable, Category = "SaveSystem|Execution")
	bool LoadGameFromSlot(const FString& SlotName);

	// ==============================================================================
	// 分页系统 (Pagination System)
	// ==============================================================================

	// 核心功能：扩充档案页数并强制落盘
	UFUNCTION(BlueprintCallable, Category = "SaveSystem|Execution")
	void UnlockNewSavePage();

	// 核心功能：清空指定页的所有物理存档，但绝对不削减页数上限
	UFUNCTION(BlueprintCallable, Category = "SaveSystem|Execution")
	void ClearSavePage(int32 PageIndex, int32 SlotsPerPage);

	// 核心功能：存档碎片整理（一键物理压缩所有空页，并自动将后续存档前移补齐）
	UFUNCTION(BlueprintCallable, Category = "SaveSystem|Execution")
	void CompactEmptySavePages(int32 SlotsPerPage);

	// ==============================================================================
	// 内部数据缓存 (Internal Data Cache)
	// ==============================================================================
public: // 调整为 public，以供 UI 面板的 BuildSaveSlotList 进行 O(1) 绝对查表读取

	// 缓存的全局存档常驻内存目录表，本质上是继承自 USaveGame 的数据容器
	// [数据功能] 仅存储所有槽位的索引元数据（名称、时间、关卡标识），不包含具体游戏状态数据。
	// [生命周期] 通过 UPROPERTY 强引用标记，强制拦截垃圾回收(GC)巡检，防止此目录表被误销毁。
	UPROPERTY()
	TObjectPtr<UMySaveRegistry> CachedRegistry;

	// ==============================================================================
	// 内部管线 (Internal Pipeline)
	// ==============================================================================
private:

	// 异步加载回调函数，当在硬盘上读取到存档文件内存后自动触发
	void OnRegistryLoaded(const FString& SlotName, const int32 UserIndex, USaveGame* LoadedGame);

	// 内部工具：每次存盘时，更新内存镜像，并把最新的“目录”写进物理硬盘
	void UpdateSaveRegistry(const FString& SlotName, FName CurrentLevelName);

	// 内部回调：底层 AsyncSaveGameToSlot 执行完毕后触发，用于向 UI 宣告结果
	void OnAsyncSaveComplete(const FString& SlotName, const int32 UserIndex, bool bSuccess);
};
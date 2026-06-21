// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SaveGame/MySaveContainer.h"
#include "MySaveSubsystem.generated.h"

// ==============================================================================
// 委托声明 (Delegates Declaration)
// ==============================================================================

// 蓝图可用的多播委托：用于异步存档写盘结束时，通知 UI 弹出“保存成功”或“保存失败”提示
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSaveFinishedSignature, bool, bSuccess);

// 蓝图可用的多播委托：当存档列表发生增删改（比如删档、扩容页数）时，通知 UI 刷新列表
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSaveRegistryChangedSignature);


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
public:

	// 核心功能：扩充档案页数并强制落盘；
	// 回调函数 UMySaveMenuWidget::OnAddPageClicked() 调用
	UFUNCTION(BlueprintCallable, Category = "SaveSystem|Execution")
	void UnlockNewSavePage();

	// 核心功能：清空指定页的所有物理存档，但绝对不削减页数上限
	// PageIndex (目标页码)；SlotsPerPage (每页容量)
	// 回调函数 UMySaveMenuWidget::OnClearPageClicked() 调用
	UFUNCTION(BlueprintCallable, Category = "SaveSystem|Execution")
	void ClearSavePage(int32 PageIndex);

	// 核心功能：存档碎片整理（一键物理压缩所有空页，并自动将后续存档前移补齐）
	// SlotsPerPage (每页容量)
	// 回调函数 UMySaveMenuWidget::OnCompactPagesClicked() 调用
	UFUNCTION(BlueprintCallable, Category = "SaveSystem|Execution")
	void CompactEmptySavePages();

	// ==============================================================================
	// 内部数据缓存 (Internal Data Cache)
	// ==============================================================================
public: 

	// 缓存的全局存档常驻内存目录表，本质上是继承自 USaveGame 的数据容器
	// 【核心定性】：它是物理硬盘里真实存档数据在内存中的“轻量级镜像 (Memory Mirror)”。
	// 【为什么必须依赖此镜像？】：
	// 1. 斩断 I/O 阻塞：物理硬盘读取极慢。如果 UI 翻页时直接读硬盘，会瞬间卡死游戏主线程引发严重掉帧；
	// 有了镜像，UI 翻页变成了纯粹的内存寻址，快了上万倍。
	// 2. 剥离重度载荷：真实存档包含几十MB的关卡和背包物品数据。镜像仅剥离提取了最轻薄的“表皮（名字、时间）”；
	// 避免了为了看一眼目录而把整座冰山搬进内存引发 GC 灾难。
	// [生命周期] 通过 UPROPERTY 强引用标记，强制拦截垃圾回收(GC)巡检，防止此目录表被误销毁。
	UPROPERTY()
	TObjectPtr<UMySaveRegistry> CachedRegistry;


	// ==============================================================================
	// 内部管线 (Internal Pipeline)
	// ==============================================================================
public:

	// 【新增原因】：跨关卡与注入时序的终极落地执行者。
	// 等 GameMode 宣告“场景构建完毕”时，调用此函数完成真实的 Actor 状态恢复。
	UFUNCTION(BlueprintCallable, Category = "SaveSystem|Execution")
	void HandlePendingLoad();

private:
	// 【新增】：强制刷新世界状态 (World Reset)
	// 无论玩家读取的是否是当前关卡，为了彻底重置物理世界（刷新怪物、重置破坏物等），现在读档一律强制执行 OpenLevel。
	// 由于旧关卡会被引擎彻底销毁，而 GameInstance (大管家) 的生命周期长于关卡永生不死，
	// 所以用这个变量作为“记忆锚点”，负责把要读的存档名安全地护送到重生后的新世界去。
	UPROPERTY()
	FString PendingLoadSlotName;

	// 异步加载回调函数，当在硬盘上读取到存档文件内存后自动触发
	void OnRegistryLoaded(const FString& SlotName, const int32 UserIndex, USaveGame* LoadedGame);

	// 内部工具：每次存盘时，更新内存镜像，并把最新的“目录”写进物理硬盘
	// UMySaveSubsystem::PerformAsyncSave 中调用
	void UpdateSaveRegistry(const FString& SlotName, FName CurrentLevelName);

	// 内部回调：底层 AsyncSaveGameToSlot 硬盘写入执行完毕后触发，用于向 UI 宣告结果
	// UMySaveSubsystem::PerformAsyncSave 中调用
	void OnAsyncSaveComplete(const FString& SlotName, const int32 UserIndex, bool bSuccess);


	// ==============================================================================
	// 安全数据访问接口 (Safe Data Access Getters)
	// ==============================================================================
public:

	// 【迪米特法则】：向外部 UI 提供安全的单页容量查询，彻底隐藏底层 Registry 实体。
	// BlueprintPure 使得蓝图调用时没有执行引脚，属于纯净的数据拉取。
	UFUNCTION(BlueprintPure, Category = "SaveSystem|Data")
	int32 GetSlotsPerPage() const;

	// 向外部提供当前玩家已解锁的总页数查询，内置极端情况的兜底防御。
	UFUNCTION(BlueprintPure, Category = "SaveSystem|Data")
	int32 GetTotalUnlockedPages() const;

	// 向外部提供游戏允许的绝对最大扩容页数上限查询。
	UFUNCTION(BlueprintPure, Category = "SaveSystem|Data")
	int32 GetMaxUnlockedPages() const;

	// 向上层 UI 安全暴露查询存档元数据的接口
	// 不加 UFUNCTION，因为蓝图虚拟机无法接住和解析一个裸的 C++ 结构体物理指针
	FSaveSlotMetaData* GetSlotMetaData(const FString& SlotName) const;
};
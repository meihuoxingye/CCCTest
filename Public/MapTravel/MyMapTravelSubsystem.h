// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MyMapTravelSubsystem.generated.h"

class UDataLayerAsset;
class UDataLayerManager;

/**
 * 负责 2.5D 横版关卡的无缝流转
 * 统筹 DataLayer 预热，并深度集成 LSP 原子级内存快照流转
 */
UCLASS()
class CCC_API UMyMapTravelSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

	// ==============================================================================
	// 生命周期与初始化 (Lifecycle & Initialization)
	// ==============================================================================
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ==============================================================================
	// 核心跳转管线 (Core Travel Pipeline)
	// ==============================================================================
public:
	// 核心跳转管线：依靠 LSP 实现无感流转，舍弃物理硬盘异步 I/O
	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void ExecuteMapTravel(FName TargetLevelName);

	// ==============================================================================
	// 视觉预热管线 (DataLayer Preloading Pipeline)
	// ==============================================================================
public:
	// 阶段 1：视觉预热
	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void PreheatZoneBackground(const UDataLayerAsset* ArtLayerAsset);

	// 阶段 2：激活当前区域
	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void ActivateZoneGameplay(const UDataLayerAsset* GameplayLayerAsset, const UDataLayerAsset* ArtLayerAsset);

	// 阶段 3：废弃远端区域
	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void EliminateZone(const UDataLayerAsset* LayerToUnload);

	// ==============================================================================
	// 内部状态锁 (Internal State Locks)
	// ==============================================================================
private:
	// 防重入锁：消除极速网络环境与脚本连点造成的管线重入崩塌
	UPROPERTY()
	bool bIsTraveling = false;

	// 5.8 官方规范缓存指针
	TWeakObjectPtr<UDataLayerManager> CachedDataLayerManager;
};
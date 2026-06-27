// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MyMapTravelSubsystem.generated.h"

class UDataLayerAsset;
class UDataLayerManager;
class UMyBiomeConfig;
class ADirectionalLight;
class AExponentialHeightFog;

// ==============================================================================
// 关卡双轨数据结构 (Dual-Track Zone Structure)
// ==============================================================================
USTRUCT(BlueprintType)
struct FZoneDataLayerPair
{
	GENERATED_BODY()

public:
	// 负责视觉：静态网格体、地形、灯光
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MapTravel")
	UDataLayerAsset* ArtLayer = nullptr;

	// 负责玩法：敌人生成器、触发器、动态物理物件
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MapTravel")
	UDataLayerAsset* GameplayLayer = nullptr;
};

/**
 * 负责 2.5D 横版关卡的无缝流转
 * 统筹 DataLayer 预热，深度集成 LSP 流转与独立加载蒙版
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

	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void ExecuteMapTravel(FName TargetLevelName);


	// ==============================================================================
	// 动态滑动窗口与流送管线 (Dynamic Sliding Window & Streaming Pipeline)
	// ==============================================================================
public:

	// 注册双轨清单
	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void RegisterZoneSequence(const TArray<FZoneDataLayerPair>& InSequence);

	// 传入美术层或玩法层均可，系统会自动定位所在大区
	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void RefreshSlidingWindow(UDataLayerAsset* TriggeredLayer);

	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void PreheatZoneBackground(const UDataLayerAsset* ArtLayerAsset);

	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void ActivateZoneGameplay(const UDataLayerAsset* GameplayLayerAsset, const UDataLayerAsset* ArtLayerAsset);

	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void EliminateZone(const UDataLayerAsset* LayerToUnload);

	// 【新增】：在卸载数据层前的安全清洗
	void SanitizeActorsForUnload(const UDataLayerAsset* ZoneToUnload);

	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void UpdateEnvironment(UMyBiomeConfig* NewBiome, ADirectionalLight* MainLight, AExponentialHeightFog* MainFog);

	// 【新增】：一键扫描并打印所有数据层的真实内存状态
	UFUNCTION(BlueprintCallable, Category = "MapTravel|Debug")
	void DebugPrintDataLayerStates();


	// ==============================================================================
	// 内部状态锁 (Internal State Locks)
	// ==============================================================================
private:

	UPROPERTY()
	bool bIsTraveling = false;

	// 区域状态防抖锁，处理玩家在交界处反复横跳的极端边缘情况
	UPROPERTY()
	UDataLayerAsset* LastActiveZone = nullptr;

	TWeakObjectPtr<UDataLayerManager> CachedDataLayerManager;

	// 双轨关卡序列
	UPROPERTY()
	TArray<FZoneDataLayerPair> ZoneSequence;

	UPROPERTY()
	UMyBiomeConfig* CurrentBiomeTarget = nullptr;

	TWeakObjectPtr<ADirectionalLight> CachedSunLight;
	TWeakObjectPtr<AExponentialHeightFog> CachedAtmosphereFog;

	FTimerHandle BiomeLerpTimer;
	float LerpAlpha = 0.0f;
	float LerpStep = 0.02f;

	FRotator StartSunRotation;
	FLinearColor StartSunColor;

	void ProcessBiomeLerpTick();
};
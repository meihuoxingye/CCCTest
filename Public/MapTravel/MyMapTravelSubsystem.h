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

	// 新地图加载完毕后的第一帧钩子
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	// ==============================================================================
	// 核心跳转管线 (Core Travel Pipeline)
	// ==============================================================================
public:

	// 增加等待时间参数
	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void ExecuteMapTravel(
		FName TargetLevelName,
		TSoftClassPtr<class UMyTransitionWidgetBase> ScreenOffUI,  // 【新增】：熄屏用的 UI 类
		float ScreenOffDuration,                       // 【新增】：熄屏动画需要播放几秒
		TSoftClassPtr<class UMyTransitionWidgetBase> CustomLoadingUI,
		float MinLoadingTime
	);

	// ==============================================================================
	// 动态滑动窗口与流送管线 (Dynamic Sliding Window & Streaming Pipeline)
	// ==============================================================================
public:

	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void RegisterZoneSequence(const TArray<FZoneDataLayerPair>& InSequence);

	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void RefreshSlidingWindow(UDataLayerAsset* TriggeredLayer);

	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void PreheatZoneBackground(const UDataLayerAsset* ArtLayerAsset);

	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void ActivateZoneGameplay(const UDataLayerAsset* GameplayLayerAsset, const UDataLayerAsset* ArtLayerAsset);

	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void EliminateZone(const UDataLayerAsset* LayerToUnload);

	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void UpdateEnvironment(UMyBiomeConfig* NewBiome, ADirectionalLight* MainLight, AExponentialHeightFog* MainFog);

	UFUNCTION(BlueprintCallable, Category = "MapTravel|Debug")
	void DebugPrintDataLayerStates();

	// ==============================================================================
	// 内部状态锁 (Internal State Locks)
	// ==============================================================================
private:

	UPROPERTY()
	bool bIsTraveling = false;

	UPROPERTY()
	UDataLayerAsset* LastActiveZone = nullptr;

	TWeakObjectPtr<UDataLayerManager> CachedDataLayerManager;

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


	// ==============================================================================
	// 同地图硬切换管线 (Intra-Map Hard Travel)
	// ==============================================================================
public:

	// 带有 UI 与强制等待的数据层切换（用于同地图内进 Boss 房等重度切换）
	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void ExecuteZoneTravelWithWait(UDataLayerAsset* TargetZone, TSoftClassPtr<class UUserWidget> CustomLoadingUI, float WaitTime);

private:

	UPROPERTY()
	class UUserWidget* ZoneLoadingWidget = nullptr;

	FTimerHandle ZoneTravelTimerHandle;

	UFUNCTION()
	void FinishZoneTravel();
};
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

USTRUCT(BlueprintType)
struct FZoneDataLayerPair
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MapTravel")
	UDataLayerAsset* ArtLayer = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MapTravel")
	UDataLayerAsset* GameplayLayer = nullptr;
};

/**
 * 负责 2.5D 横版关卡的无缝流转
 * 采用 GameViewport 顶级注入与跨界托管，实现绝对无缝
 */
UCLASS()
class CCC_API UMyMapTravelSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

	// ==============================================================================
	// 生命周期与初始化
	// ==============================================================================
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	// ==============================================================================
	// 核心跳转管线
	// ==============================================================================
public:
	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void ExecuteMapTravel(FName TargetLevelName, TSoftClassPtr<class UUserWidget> CustomLoadingUI = nullptr, float MinLoadingTime = 2.0f);

	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void ExecuteZoneTravelWithWait(UDataLayerAsset* TargetZone, TSoftClassPtr<class UUserWidget> CustomLoadingUI, float WaitTime);

	// ==============================================================================
	// 动态滑动窗口与流送管线
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
	// 内部工具函数
	// ==============================================================================
private:
	APlayerController* GetRealPlayerController(UWorld* World) const;

	// ==============================================================================
	// 内部状态锁与流送资产
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
	// 物理视口管理与落地回调
	// ==============================================================================
private:
	FTimerHandle ArrivalTimerHandle;
	FTimerHandle ZoneTravelTimerHandle;

	// 仅用于同地图内的硬切 UI 缓存 (不经历跨界清洗)
	UPROPERTY()
	class UUserWidget* ZoneLoadingWidget = nullptr;
	TSharedPtr<class SWidget> ZoneSlateWidget;

	UFUNCTION()
	void FinishMapTravel();

	UFUNCTION()
	void FinishZoneTravel();
};
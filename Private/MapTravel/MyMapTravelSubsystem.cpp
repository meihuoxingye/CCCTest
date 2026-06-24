// Fill out your copyright notice in the Description page of Project Settings.

#include "MapTravel/MyMapTravelSubsystem.h"
#include "Engine/World.h"
#include "WorldPartition/DataLayer/DataLayerAsset.h"
#include "WorldPartition/DataLayer/DataLayerManager.h"


// ==============================================================================
// 生命周期与初始化 (Lifecycle & Initialization)
// ==============================================================================
#pragma region

void UMyMapTravelSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UWorld* World = GetWorld())
	{
		// 【核心修复】：UDataLayerManager 不再是 Subsystem，必须通过静态函数获取
		CachedDataLayerManager = UDataLayerManager::GetDataLayerManager(World);
	}

	bIsTraveling = false;
}

void UMyMapTravelSubsystem::Deinitialize()
{
	CachedDataLayerManager.Reset();
	Super::Deinitialize();
}

#pragma endregion


// ==============================================================================
// 核心跳转管线 (Core Travel Pipeline)
// ==============================================================================
#pragma region

void UMyMapTravelSubsystem::ExecuteMapTravel(FName TargetLevelName)
{
	if (bIsTraveling)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World) || TargetLevelName.IsNone())
	{
		return;
	}

	bIsTraveling = true;
	World->ServerTravel(TargetLevelName.ToString());
}

#pragma endregion


// ==============================================================================
// 视觉预热管线 (DataLayer Preloading Pipeline)
// ==============================================================================
#pragma region

void UMyMapTravelSubsystem::PreheatZoneBackground(const UDataLayerAsset* ArtLayerAsset)
{
	if (CachedDataLayerManager.IsValid() && ArtLayerAsset)
	{
		CachedDataLayerManager->SetDataLayerRuntimeState(ArtLayerAsset, EDataLayerRuntimeState::Loaded);
	}
}

void UMyMapTravelSubsystem::ActivateZoneGameplay(const UDataLayerAsset* GameplayLayerAsset, const UDataLayerAsset* ArtLayerAsset)
{
	if (CachedDataLayerManager.IsValid())
	{
		if (ArtLayerAsset)
		{
			CachedDataLayerManager->SetDataLayerRuntimeState(ArtLayerAsset, EDataLayerRuntimeState::Activated);
		}
		if (GameplayLayerAsset)
		{
			CachedDataLayerManager->SetDataLayerRuntimeState(GameplayLayerAsset, EDataLayerRuntimeState::Activated);
		}
	}
}

void UMyMapTravelSubsystem::EliminateZone(const UDataLayerAsset* LayerToUnload)
{
	if (CachedDataLayerManager.IsValid() && LayerToUnload)
	{
		CachedDataLayerManager->SetDataLayerRuntimeState(LayerToUnload, EDataLayerRuntimeState::Unloaded);
	}
}

#pragma endregion
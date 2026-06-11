// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SaveGame/MySaveGame.h"
#include "MySaveSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSaveFinishedSignature, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSaveRegistryChangedSignature);

DECLARE_MULTICAST_DELEGATE_OneParam(FOnGameSavingSignature, UMySaveGame* /*SaveObj*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGameLoadingSignature, UMySaveGame* /*SaveObj*/);

// ==============================================================================
// 全局存档统筹子系统 (Global Save Subsystem)
// ==============================================================================
UCLASS()
class CCC_API UMySaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ==============================================================================
	// 核心接口 (Core Interfaces)
	// ==============================================================================
	UPROPERTY(BlueprintAssignable, Category = "SaveSystem|Events")
	FOnSaveFinishedSignature OnSaveFinished;

	UPROPERTY(BlueprintAssignable, Category = "SaveSystem|Events")
	FOnSaveRegistryChangedSignature OnSaveRegistryChanged;

	FOnGameSavingSignature OnGameSaving;
	FOnGameLoadingSignature OnGameLoading;

	// 【新增】：异步加载注册表入口
	void PreloadRegistry();

	UFUNCTION(BlueprintCallable, Category = "SaveSystem|Execution")
	void PerformAsyncSave(const FString& SlotName);

	UFUNCTION(BlueprintCallable, Category = "SaveSystem|Execution")
	bool DeleteSaveSlot(const FString& SlotName);

	UFUNCTION(BlueprintCallable, Category = "SaveSystem|Execution")
	bool LoadGameFromSlot(const FString& SlotName);

	// 【重构】：改为 O(1) 极速读内存
	UFUNCTION(BlueprintCallable, Category = "SaveSystem|Query")
	TArray<FSaveSlotMetaData> GetSaveSlotList();

private:
	// 【新增】：存档注册表的常驻内存镜像
	UPROPERTY()
	TObjectPtr<UMySaveRegistry> CachedRegistry;

	// 【新增】：异步加载回调
	void OnRegistryLoaded(const FString& SlotName, const int32 UserIndex, USaveGame* LoadedGame);

	// ==============================================================================
	// 内部管线 (Internal Pipeline)
	// ==============================================================================
	void UpdateSaveRegistry(const FString& SlotName, FName CurrentLevelName);

	void OnAsyncSaveComplete(const FString& SlotName, const int32 UserIndex, bool bSuccess);
};
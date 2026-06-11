// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SaveGame/MySaveGame.h"
#include "MySaveSubsystem.generated.h"

// 供 UI 监听的异步完成委托
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSaveFinishedSignature, bool, bSuccess);

// 【修复新增】：声明存档名册变动大喇叭（无参数）
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSaveRegistryChangedSignature);

// 【核心解耦：全局读写总线】
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

	// 【修复新增】：实例化删档大喇叭
	UPROPERTY(BlueprintAssignable, Category = "SaveSystem|Events")
	FOnSaveRegistryChangedSignature OnSaveRegistryChanged;

	FOnGameSavingSignature OnGameSaving;
	FOnGameLoadingSignature OnGameLoading;

	UFUNCTION(BlueprintCallable, Category = "SaveSystem|Execution")
	void PerformAsyncSave(const FString& SlotName);

	// 【修复新增】：物理删除指定存档接口
	UFUNCTION(BlueprintCallable, Category = "SaveSystem|Execution")
	bool DeleteSaveSlot(const FString& SlotName);

	UFUNCTION(BlueprintCallable, Category = "SaveSystem|Execution")
	bool LoadGameFromSlot(const FString& SlotName);

	UFUNCTION(BlueprintCallable, Category = "SaveSystem|Query")
	TArray<FSaveSlotMetaData> GetSaveSlotList();

private:
	// ==============================================================================
	// 内部管线 (Internal Pipeline)
	// ==============================================================================

	void UpdateSaveRegistry(const FString& SlotName, FName CurrentLevelName);

	void OnAsyncSaveComplete(const FString& SlotName, const int32 UserIndex, bool bSuccess);
};
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SaveGame/MySaveGame.h"
#include "MySaveSubsystem.generated.h"

// 供 UI 监听的异步完成委托
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSaveFinishedSignature, bool, bSuccess);

// 【核心解耦：全局读写总线】
// 不使用 DYNAMIC，使用原生 C++ 多播委托，性能最高，且不需要经过反射系统
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

	// 【新增】：存档广播频道（下达收集数据的指令）
	FOnGameSavingSignature OnGameSaving;

	// 【新增】：读档广播频道（下达分发数据的指令）
	FOnGameLoadingSignature OnGameLoading;

	UFUNCTION(BlueprintCallable, Category = "SaveSystem|Execution")
	void PerformAsyncSave(const FString& SlotName);

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
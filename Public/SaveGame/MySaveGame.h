// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "SkillSystem/SkillPointSubsystem.h" 
#include "MySaveGame.generated.h"

// ==============================================================================
// 存档元数据 (Save Slot Metadata)
// ==============================================================================

/**
 * 极轻量级数据结构，专门用于在 UI 列表中快速展示，无需加载真实物理世界数据
 */
USTRUCT(BlueprintType)
struct FSaveSlotMetaData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "SaveData|Meta")
	FString SlotName;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "SaveData|Meta")
	FDateTime SaveTime;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "SaveData|Meta")
	FName LevelName;
};

// ==============================================================================
// 全局存档注册表 (Global Save Registry)
// ==============================================================================

/**
 * 永远存放在固定槽位 "GlobalSaveRegistry" 中的小型清单文件
 */
UCLASS()
class CCC_API UMySaveRegistry : public USaveGame
{
	GENERATED_BODY()

public:
	// 记录所有已存在的存档位及其元数据
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "SaveData|Registry")
	TMap<FString, FSaveSlotMetaData> SaveSlots;
};

// ==============================================================================
// 核心存档数据容器 (Core Save Game Container)
// ==============================================================================

/**
 * 包含所有需要持久化的重量级游戏数据，使用异步 I/O 写入
 */
UCLASS()
class CCC_API UMySaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "SaveData|Player")
	FTransform PlayerTransform;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "SaveData|Squad")
	TMap<FName, FCharacterSPData> SavedSquadSPMap;

	// 记录已被击杀的精英怪或已拾取的唯一道具的 GUID
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "SaveData|World")
	TSet<FGuid> EliminatedActorIDs;
};
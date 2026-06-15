// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
// 看！不需要包含任何外部的业务头文件了，做到了绝对解耦！
#include "MySaveDataTypes.generated.h"

// ==============================================================================
// 1. 角色专用数据块 (游牧数据)
// ==============================================================================
USTRUCT(BlueprintType)
struct CCC_API FCharacterSaveData
{
	GENERATED_BODY()

public:
	FCharacterSaveData();

	// 直接存储基础物理数值，而不是强耦合具体的 SP 结构体
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	int32 CurrentSP;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	int32 MaxSP;

	// 未来可以加：
	// UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	// float CurrentHP;
};

// ==============================================================================
// 2. 地图专用数据块 (固定资产数据)
// ==============================================================================
USTRUCT(BlueprintType)
struct CCC_API FLevelSaveData
{
	GENERATED_BODY()

public:
	FLevelSaveData();

	// 记录这个关卡里哪些怪已经被杀死了（存怪物的 ID）
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	TArray<FName> DeadEnemies;

	// 记录这个关卡里哪些机关/门被打开了
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	TArray<FName> OpenedDoors;
};

// ==============================================================================
// 3. 游戏全局数据块 (比如时间、金币、任务进度)
// ==============================================================================
USTRUCT(BlueprintType)
struct CCC_API FGlobalSaveData
{
	GENERATED_BODY()

public:
	FGlobalSaveData();

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	int32 TotalGold;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FName CurrentMainQuestID;
};
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

	// 保存玩家在 3D 世界中的精确坐标、旋转和缩放。
	// 因为使用了 FTransform（而非 FVector），角色复活/读档时，面朝的方向也会被完美还原。
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FTransform PlayerTransform;

	// 玩家存盘时所在的关卡名。
	// 可直接喂给 UI 上的 TextBlock 用于显示“当前章节/区域”，或者供读取统筹器进行无缝切图。
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FName SavedLevelName;

	// 暂时没用到
	// 记录已被击杀的精英怪或已拾取的唯一道具的 GUID
	// 【现代独立游戏标准的“生死簿”】：
	// 在你的游戏世界里，切关卡或读档重生时，所有怪物/宝箱在出生前，都要来向这个集合查阅：“我死过了吗？”
	// 如果怪物发现自己的 GUID 已经被登记在案，就会执行自行销毁（Destroy），从而完美防止精英怪重复刷血或宝箱被无限开。
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	TSet<FGuid> EliminatedActorIDs;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	int32 TotalGold;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FName CurrentMainQuestID;
};
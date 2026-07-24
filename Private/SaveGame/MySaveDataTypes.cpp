// Fill out your copyright notice in the Description page of Project Settings.

#include "SaveGame/MySaveDataTypes.h"

// ==============================================================================
// 1. 角色专用数据块 (游牧数据)
// ==============================================================================
#pragma region

FCharacterSaveData::FCharacterSaveData()
{
	// 给基础数值赋安全的默认初值
	CurrentSP = 0;
	MaxSP = 0;
}

#pragma endregion

// ==============================================================================
// 2. 地图专用数据块 (固定资产数据)
// ==============================================================================
#pragma region

FLevelSaveData::FLevelSaveData()
{
	DeadEnemies.Empty();
	OpenedDoors.Empty();
}

#pragma endregion

// ==============================================================================
// 3. 游戏全局数据块 (系统锚点与全局状态)
// ==============================================================================
#pragma region

FGlobalSaveData::FGlobalSaveData()
{
	// FTransform 默认提供单位矩阵 (0,0,0) 位置与 (1,1,1) 缩放
	SavedLevelName = NAME_None;
	EliminatedActorIDs.Empty();
	TotalGold = 0;
	CurrentMainQuestID = NAME_None;
}

#pragma endregion
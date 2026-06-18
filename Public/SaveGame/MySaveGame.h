// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
// 【绝对解耦】：仅包含纯净的图纸，不包含任何业务类
#include "SaveGame/MySaveDataTypes.h" 
#include "MySaveGame.generated.h"


// ==============================================================================
// 全局存档注册表 (Global Save Registry)
// ==============================================================================

/** * 继承 USaveGame 的 UMySaveRegistry 的数据类一
 * 永远存放在固定槽位 "GlobalSaveRegistry" 中的小型清单文件
 */
UCLASS()
class CCC_API UMySaveRegistry : public USaveGame
{
	GENERATED_BODY()

public:
	// 记录所有已存在的存档位及其元数据，继承 USaveGame 的 UMySaveRegistry (常驻内存，极小文件)；
	// Key 为物理档位名，Value 为对应的极轻量 UI 展示数据 FSaveSlotMetaData（自定义数据结构，只包含时间和名称，不包含角色属性、位置）。
	// 【架构核武】：这不仅是数据的清单，它更是对玩家物理硬盘的“防暴力遍历锁”。
	// 虚幻引擎底层去遍历硬盘文件开销极大，有了这个固定的全局词典，永远只读它一次就能掌握所有存档状况。
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "SaveData|Registry")
	TMap<FString, FSaveSlotMetaData> SaveSlots;

	// 物理写盘记录：玩家当前解锁了几页存档 (初始给3页)
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "SaveData|Registry")
	int32 UnlockedPages = 3;

	// 【架构加固】：将每页容量收归底层物理数据字典管理，彻底与 UI 解耦
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "SaveData|Registry")
	int32 SlotsPerPage = 5;

	// 【架构加固】：将最低保底页数收归管理，彻底消灭底层硬编码
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "SaveData|Registry")
	int32 MinUnlockedPages = 3;

	// 【架构加固】：将最高页数上限收归管理，防止玩家无限拓荒撑爆内存
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "SaveData|Registry")
	int32 MaxUnlockedPages = 50;
};

// ==============================================================================
// 核心存档数据容器 (Core Save Game Container)
// ==============================================================================

/** * USaveGame 的数据类二
 * 包含所有需要持久化的重量级游戏数据，使用异步 I/O 写入
 * 铁律：本类中禁止声明任何独立的基础变量，只允许装载 MySaveDataTypes 中定义的数据块！
 */
UCLASS()
class CCC_API UMySaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	// UMySaveGame (实体大卡车，巨大文件)；FGlobalSaveData 自定义数据结构；
	// 记录绝对物理状态（玩家坐标、当前关卡名）。这些是底层的“硬基建”数据，由 UMySaveSubsystem 亲自保管。
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "SaveData|Global")
	FGlobalSaveData GlobalDataBlock;

	// UMySaveGame (实体大卡车，巨大文件)
	// 各个业务部门（技能点、背包）把自己的数据打包成无逻辑的纸箱子（JSON 字符串）扔在这里。
	// 只要存成了字符串，大管家 UMySaveSubsystem 就再也不需要认识任何业务系统的头文件了！
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "SaveData|Universal")
	TMap<FName, FString> UniversalArchives;
};
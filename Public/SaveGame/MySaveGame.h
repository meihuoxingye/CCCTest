// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "SkillSystem/SkillPointSubsystem.h" 
#include "MySaveGame.generated.h"

// ==============================================================================
// 存档元数据 (Save Slot Metadata)
// ==============================================================================

/** * 极轻量级数据结构，专门用于在 UI 列表中快速展示，无需加载真实物理世界数据
 * * 为什么要单独创建一个数据文件：
 * 虚幻的底层序列化系统（ISaveGameSystem）极其死板。当你调用 AsyncSaveGameToSlot 或 AsyncLoadGameFromSlot 去读写硬盘时
 * 引擎只认继承自 USaveGame 的类对象
 */

USTRUCT(BlueprintType)
struct FSaveSlotMetaData
{
	GENERATED_BODY()

	// 档位唯一标识符（即保存在物理硬盘上的 .sav 文件名，如 "Save_2026..."）
	// UI 点击某个存档卡片准备读取时，就是拿着这个名字去求子系统的。
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "SaveData|Meta")
	FString SlotName;

	// 存档建立的绝对时间戳。
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "SaveData|Meta")
	FDateTime SaveTime;

	// 玩家存盘时所在的关卡名。
	// 可直接喂给 UI 上的 TextBlock 用于显示“当前章节/区域”，或者供读取统筹器进行无缝切图。
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "SaveData|Meta")
	FName LevelName;
};


// ==============================================================================
// 全局存档注册表 (Global Save Registry)
// ==============================================================================

/** 
 * 继承 USaveGame 的数据类一
 * 永远存放在固定槽位 "GlobalSaveRegistry" 中的小型清单文件
 */
UCLASS()
class CCC_API UMySaveRegistry : public USaveGame
{
	GENERATED_BODY()

public:
	// 记录所有已存在的存档位及其元数据
	// Key 为物理档位名，Value 为对应的极轻量 UI 展示数据。
	// 【架构核武】：这不仅是数据的清单，它更是对玩家物理硬盘的“防暴力遍历锁”。
	// 虚幻引擎底层去遍历硬盘文件开销极大，有了这个固定的全局词典，永远只读它一次就能掌握所有存档状况。
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "SaveData|Registry")
	TMap<FString, FSaveSlotMetaData> SaveSlots;

	// 物理写盘记录：玩家当前解锁了几页存档 (初始给3页)
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "SaveData|Registry")
	int32 UnlockedPages = 3;
};


// ==============================================================================
// 核心存档数据容器 (Core Save Game Container)
// ==============================================================================

/** 
 * 继承 USaveGame 的数据类二
 * 包含所有需要持久化的重量级游戏数据，使用异步 I/O 写入
 */

UCLASS()
class CCC_API UMySaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	// 保存玩家在 3D 世界中的精确坐标、旋转和缩放。
	// 因为使用了 FTransform（而非 FVector），角色复活/读档时，面朝的方向也会被完美还原。
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "SaveData|Player")
	FTransform PlayerTransform;

	// 跨系统解耦数据对接：把你在 SP 子系统中计算的队伍各角色技能点状态，原封不动地“拓印”下来。
	// 读档时，再将这个 Map 一脚踢回给 SP 子系统，实现数值的无缝恢复。
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "SaveData|Squad")
	TMap<FName, FCharacterSPData> SavedSquadSPMap;

	// 记录已被击杀的精英怪或已拾取的唯一道具的 GUID
	// 【现代独立游戏标准的“生死簿”】：
	// 在你的游戏世界里，切关卡或读档重生时，所有怪物/宝箱在出生前，都要来向这个集合查阅：“我死过了吗？”
	// 如果怪物发现自己的 GUID 已经被登记在案，就会执行自行销毁（Destroy），从而完美防止精英怪重复刷血或宝箱被无限开。
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "SaveData|World")
	TSet<FGuid> EliminatedActorIDs;
};
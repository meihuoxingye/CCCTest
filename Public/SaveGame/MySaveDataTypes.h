/// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
// 看！不需要包含任何外部的业务头文件了，做到了绝对解耦！
#include "MySaveDataTypes.generated.h"

// ==============================================================================
// 0. 存档元数据 (Save Slot Metadata)
// ==============================================================================

/** * 极轻量级数据结构，专门用于在 UI 列表中快速展示，无需加载真实物理世界数据
 * * 【归属关系】：
 * - 宿主：UMySaveRegistry (登记本)
 * - 存储形式：作为 SaveSlots 字典的 Value 存在。
 * - 物理落盘：仅存在于极小的 GlobalSaveRegistry.sav 文件中，绝对不会进入真实存档文件。
 * * 为什么要单独创建一个数据文件：
 * 虚幻的底层序列化系统（ISaveGameSystem）极其死板。当你调用 AsyncSaveGameToSlot 或 AsyncLoadGameFromSlot 去读写硬盘时
 * 引擎只认继承自 USaveGame 的类对象
 */
USTRUCT(BlueprintType)
struct CCC_API FSaveSlotMetaData
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
// 1. 角色专用数据块 (游牧数据)
// ==============================================================================

/** * 【归属关系】：
 * - 宿主：具体的业务部门（例如：技能点子系统 USkillPointSubsystem）。
 * - 存储形式：业务部门会用 JSON 榨汁机，把这个结构体压扁成纯文本的 FString 字符串。
 * - 物理落盘：被大管家当作无差别黑盒货物，扔进大卡车 (UMySaveGame) 的万能集装箱 (UniversalArchives) 里存盘。大管家不认识此数据。
 */
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

/** * 【归属关系】：
 * - 宿主：地图状态管理子系统或各种关卡管理器。
 * - 存储形式：同上，被业务部门自己压扁成 JSON 格式的 FString 字符串。
 * - 物理落盘：混在万能集装箱 (UniversalArchives) 里存盘，与其他业务数据物理隔离。
 */
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

/** * 【归属关系】：
 * - 宿主：存档大管家（UMySaveSubsystem）亲自持有。
 * - 存储形式：作为大卡车 (UMySaveGame) 的核心 VIP 变量 GlobalDataBlock 原生存在，不经过 JSON 转换。
 * - 物理落盘：因为包含最底层的世界锚点（坐标、关卡），由大管家亲自收集数据并直接写入大卡车里。
 */
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

	// 暂时没用
	// 总金币 / 货币
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	int32 TotalGold;

	// 暂时没用
	// 当前主线任务 ID
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FName CurrentMainQuestID;
};
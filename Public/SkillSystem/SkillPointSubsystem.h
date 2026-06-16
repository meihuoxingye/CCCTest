// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
// 【引入纯净块】：只认数据格式，不认系统，实现绝对解耦
#include "SaveGame/MySaveDataTypes.h" 
// 【修复】：必须要让本系统认识契约长什么样！(注意一定要加在 .generated.h 上面)
#include "SaveGame/MySavableInterface.h"
#include "SkillPointSubsystem.generated.h"


// 删掉 DYNAMIC，使用原生多播委托
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSPChangedSignature, FName /*CharacterID*/, float /*NewSPPercent*/);

/**
 * */

 // 【新增】：专门用于序列化 JSON 的数据包裹
USTRUCT()
struct FSkillPointSavePackage
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<FName, FCharacterSaveData> SPDataMap;
};

 // 角色技能点数据结构体，包含当前 SP、上限、恢复速率等信息
USTRUCT(BlueprintType)
struct FCharacterSPData
{
	// 虚幻反射系统宏，允许引擎识别这个结构体
	GENERATED_BODY()

	// 技能点上限
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SP")
	float MaxSP = 100.0f;

	// 【核心】快照点数：记录上次一操作（扣除、冻结、升级）时确切定格的技能点
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SP")
	float SavedSP = 0.0f;

	// 作为时间戳，记录上次同步技能点的游戏时间
	// 采用 double 类型匹配 UE5 Large World Coordinates (LWC) 高精度时间轴
	UPROPERTY(BlueprintReadWrite, Category = "SP")
	double LastSyncGameTime = 0.0;

	// 技能点每秒恢复速度
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SP")
	float RegenRate = 10.0f;

	// 状态锁：用于剧情过场或进入安全区时，停止技能点恢复
	UPROPERTY(BlueprintReadWrite, Category = "SP")
	bool bIsRegenFrozen = false;
};



UCLASS()
class CCC_API USkillPointSubsystem : public UWorldSubsystem, public IMySavableInterface
{
	GENERATED_BODY()


	// ==============================================================================
	// 核心生命周期与初始化 (Core Lifecycle & Initialization)
	// ==============================================================================
public:
	// 系统初始化，预设一个测试角色并加入哈希表
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// 系统销毁时，切记清除委托监听，防止换地图时产生野指针
	virtual void Deinitialize() override;

	// 为处理不可预测的突发事件（如技能释放、装备升级），使用委托
	// 不经过虚幻反射系统，只在 C++ 使用的原生多播委托，性能略好一些
	// 原生委托不需要也不能加 UPROPERTY，它就是纯 C++ 变量
	FOnSPChangedSignature OnSPChanged;


	// ==============================================================================
	// 核心状态查询 (Core State Query)
	// ==============================================================================
public:
	// --- 核心只读函数，全部标记为 const，实现 Const Correctness ---

	// 惰性求值，获取实时 SP
	UFUNCTION(BlueprintPure, Category = "Squad")
	float GetCalculatedCharacterSP(FName CharacterID) const;

	// 获取当前SP百分比（0.0~1.0，专门给UI进度条直接绑定用）
	UFUNCTION(BlueprintPure, Category = "Squad")
	float GetCharacterSPPercent(FName CharacterID) const;

	// 向下取整，如把 45.8 变成整数 45，给 UI 文本显示用
	// 优化：标记为 const 函数
	UFUNCTION(BlueprintPure, Category = "Squad")
	int32 GetCurrentSPAsInt(FName CharacterID) const;


	// ==============================================================================
	// 技能点消费与管控 (SP Consumption & Control)
	// ==============================================================================
public:
	// --- 修改状态的函数，保持非 const ---
	// 释放技能时的扣费系统
	// 设为布尔类型，将“检查”与“扣费”合二为一
	UFUNCTION(BlueprintCallable, Category = "Squad")
	bool ConsumeCharacterSP(FName CharacterID, float Amount);

	// 设置角色的技能点恢复功能是解冻还是冻结 (用于安全区和存读档)
	UFUNCTION(BlueprintCallable, Category = "Squad")
	void SetCharacterRegenFrozen(FName CharacterID, bool bFreeze);

	// 一键控制全场所有角色的技能点恢复功能是解冻还是冻结 
	UFUNCTION(BlueprintCallable, Category = "Squad")
	void SetAllCharactersRegenFrozen(bool bFreeze);


	// ==============================================================================
	// 存档与时间轴同步 (Save System & Timeline Sync)
	// ==============================================================================
public:
	// 为存档做准备，强制把所有角色的技能点数据同步到当前时间点，并冻结技能点恢复功能
	UFUNCTION(BlueprintCallable, Category = "Squad|SaveSystem")
	void PrepareForSave();

	// 读档后对齐时间轴，解冻技能点恢复功能
	UFUNCTION(BlueprintCallable, Category = "Squad|SaveSystem")
	void PostLoadSync();

	// 【注入动作】：大管家读档时调用，强行注入纯净数据（替代原有的 HandleGameLoading）
	// 读档大管家卸货时，将剥离了业务逻辑的基础数字一脚踢给本系统进行重新组装
	UFUNCTION(BlueprintCallable, Category = "Squad|SaveSystem")
	void InjectSaveData(const TMap<FName, FCharacterSaveData>& InArchive);

	// 【提取动作】：大管家存档时调用，剥离业务逻辑生成纯净切片（替代原有的 HandleGameSaving）
	// 存档大管家发车时，向本系统索要一份用于写盘的纯净基础数据
	UFUNCTION(BlueprintCallable, Category = "Squad|SaveSystem")
	TMap<FName, FCharacterSaveData> ExtractSaveData();


	// ==============================================================================
	// 小队成员数据管理 (Squad Member Data Management)
	// ==============================================================================
public:
	// 外部调用：新角色加入，分配初始数据（如果已存在则不处理）
	UFUNCTION(BlueprintCallable, Category = "Squad")
	void AddNewCharacterToSquad(FName CharacterID, float MaxSP, float InitialSP, float RegenRate);

	// 外部调用：升级改变 SP 上限
	UFUNCTION(BlueprintCallable, Category = "Squad")
	void UpdateCharacterConfig(FName CharacterID, float NewMaxSP, float NewRegenRate);

	// 外部调用：角色永久离开小队，彻底清空哈希表里的这个 Key，释放物理内存
	UFUNCTION(BlueprintCallable, Category = "Squad")
	void RemoveCharacterFromSquad(FName CharacterID);


	// ==============================================================================
	// 内部数据状态 (Internal Data State)
	// ==============================================================================
public:
	// 技能点数据哈希表，Key 是角色 ID（FName），Value 是技能点数据结构体
	UPROPERTY(BlueprintReadWrite, Category = "Squad")
	TMap<FName, FCharacterSPData> SquadSPMap;


	// ==============================================================================
	// 存档接口契约 (ISavableInterface Implementation)
	// ==============================================================================
public:
	virtual FName GetModuleName() const override;
	virtual FString ExtractUniversalData() override;
	virtual void InjectUniversalData(const FString& InJSONString) override;
};
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MySquadSubsystem.generated.h"


// 这是告诉其他文件：有一个叫 LogSquadSystem 的频道可以用
DECLARE_LOG_CATEGORY_EXTERN(LogSquadSystem, Log, All);

// 一个小组的数据结构
USTRUCT()
struct FSquadGroup
{
    GENERATED_BODY()

    // 使用弱指针，当角色被销毁了，弱指针会自动意识到“目标已消失”
    // 可用于 RemoveAll 的逻辑判断
    UPROPERTY()
    TArray<TWeakObjectPtr<class ABaseCharacter>> Members;

    // 本小组的锚点位置，组队搜索的圆形位置
    FVector AnchorLocation;
    float YTimer = 0.f;

    // 标记该小组是否已进入战斗状态（发现目标）
    bool bIsAggro = false;
};

/**
 * 
 */
UCLASS()
class CCC_API UMySquadSubsystem : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
    // 外部调用，将角色注册到待组队池中
    // 基础角色 BeginPlay 已调用
    void RegisterCandidate(class ABaseCharacter* Character);
    // 外部调用，从待组队池或已形成的小组中注销，或解散角色所在小组
    // 基础角色 EndPlay 已调用
    void UnregisterCharacter(class ABaseCharacter* Character);
    // 从已形成的小组池里的小组中移除角色，若移除后只有一人或以下则顺便解散小组
    void RemoveFromGroup(ABaseCharacter* Character);

    FVector GetTacticalLocation(class ABaseCharacter* Character);

    // 让控制器在发现角色时通知子系统
    void SetGroupAggro(class ABaseCharacter* Member, bool bNewAggro);

    // FTickableGameObject 接口实现
    virtual void Tick(float DeltaTime) override;
    virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UMySquadSubsystem, STATGROUP_Tickables); }

    /** * 公开接口：允许 AI 控制器获取当前的候选人列表进行避障计算
     * 使用 FORCEINLINE 保证性能，const 确保安全性
     */
    FORCEINLINE const TArray<TWeakObjectPtr<class ABaseCharacter>>& GetCandidates() const { return Candidates; }

private:
    // 待组队池
    // 使用弱指针，当角色被销毁了，弱指针会自动意识到“目标已消失”
    // 
    UPROPERTY()
    TArray<TWeakObjectPtr<class ABaseCharacter>> Candidates;

    // 已形成的小组
    UPROPERTY()
    TArray<FSquadGroup> ActiveGroups;

    // 组队逻辑
    void UpdateGroupingLogic();
    // 
    void UpdateMovementLogic(float DeltaTime);
	
    /** * 计算指定小组所有成员的几何中心点（重心） */
    FVector GetGroupCenter(const FSquadGroup& Group) const;

protected:
    // 空间分块，将世界划分为多个栅格，栅格宽度（厘米）建议设为组队半径的 1.2~1.5 倍
    // 过大会导致去检测远距离的目标，过小会多次查询哈希表降低性能
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "User-defined Parameters")
    float GridSize = 1000.f;
};
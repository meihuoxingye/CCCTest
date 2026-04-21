// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MySquadSubsystem.generated.h"


// 这是告诉其他文件：有一个叫 LogSquadSystem 的频道可以用
DECLARE_LOG_CATEGORY_EXTERN(LogSquadSystem, Log, All);

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

    /** * 公开接口：允许 AI 控制器获取当前的候选人列表进行避障计算
    * 使用 FORCEINLINE 保证性能，const 确保安全性
    */
    FORCEINLINE const TArray<TWeakObjectPtr<class ABaseCharacter>>& GetCandidates() const { return Candidates; }

    // FTickableGameObject 接口实现
    virtual void Tick(float DeltaTime) override;
    virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UMySquadSubsystem, STATGROUP_Tickables); }

private:
    // 待组队池
    // 使用弱指针，当角色被销毁了，弱指针会自动意识到“目标已消失”
    UPROPERTY()
    TArray<TWeakObjectPtr<class ABaseCharacter>> Candidates;

    // 已形成的小组
    UPROPERTY()
    TArray<class FSquadGroup> ActiveGroups;

    // 组队逻辑
    void UpdateGroupingLogic();


protected:
    // 空间分块，将世界划分为多个栅格，栅格宽度（厘米）建议设为组队半径的 1.2~1.5 倍
    // 过大会导致去检测远距离的目标，过小会多次查询哈希表降低性能
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "User-defined Parameters")
    float GridSize = 1000.f;

    // 在 Tick 中用于计算累计时间，达到一定值时执行组队逻辑
    // 为什么不定义局部静态变量而选择使用成员变量
    // UE 编辑器本身也是一个世界，游戏世界也是一个世界，这两个世界会共享同一个计时器
    // 编辑器世界没怪，它跑完一次 Tick 把计时器清零了；轮到游戏世界时，计时器永远到不了 0.5s，导致游戏逻辑永远不触发
    float GroupingTimer = 0.f;
};
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MySquadSubsystem.generated.h"


USTRUCT()
struct FSquadGroup
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<TWeakObjectPtr<class ABaseCharacter>> Members;

    FVector AnchorLocation;
    float YTimer = 0.f;
};

/**
 * 
 */
UCLASS()
class CCC_API UMySquadSubsystem : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
    void RegisterCandidate(class ABaseCharacter* Character);
    void UnregisterCandidate(class ABaseCharacter* Character);
    FVector GetTacticalLocation(class ABaseCharacter* Character);

    // FTickableGameObject 接口实现
    virtual void Tick(float DeltaTime) override;
    virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UMySquadSubsystem, STATGROUP_Tickables); }

    /** * 公开接口：允许 AI 控制器获取当前的候选人列表进行避障计算
     * 使用 FORCEINLINE 保证性能，const 确保安全性
     */
    FORCEINLINE const TArray<TWeakObjectPtr<class ABaseCharacter>>& GetCandidates() const { return Candidates; }

private:
    UPROPERTY()
    TArray<TWeakObjectPtr<class ABaseCharacter>> Candidates; // 待组队池

    UPROPERTY()
    TArray<FSquadGroup> ActiveGroups; // 已形成的小组

    void UpdateGroupingLogic();
    void UpdateMovementLogic(float DeltaTime);
	
    /** * 计算指定小组所有成员的几何中心点（重心） */
    FVector GetGroupCenter(const FSquadGroup& Group) const;
};

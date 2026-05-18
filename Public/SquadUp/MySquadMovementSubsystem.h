// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MySquadMovementSubsystem.generated.h"

UCLASS()
class CCC_API UMySquadMovementSubsystem : public UWorldSubsystem, public FTickableGameObject
{
    GENERATED_BODY()

public:
    // 优化 3：标记为 const 保证只读安全。全图 AI 每帧调用它时，纯查表 O(1) 开销极低
    FVector GetTacticalLocation(class ABaseCharacter* Character) const;

    // --- FTickableGameObject 接口实现 ---
    virtual void Tick(float DeltaTime) override;
    virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UMySquadMovementSubsystem, STATGROUP_Tickables); }

private:
    void UpdateMovementLogic(float DeltaTime);

    // 优化 1：使用弱指针懒加载缓存玩家 Pawn，彻底消灭每帧 GetPlayerPawn 全局遍历！
    UPROPERTY()
    TWeakObjectPtr<class APawn> CachedPlayerPawn;

    // 优化 2：战术坐标快速缓存表。只存活一帧，供本帧所有 AI 极速读取
    // 因为每帧都会清空，所以直接用原始指针做 Key 是绝对安全的，且寻址最快
    TMap<class ABaseCharacter*, FVector> TacticalLocationCache;
};
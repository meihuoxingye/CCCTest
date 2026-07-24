// Fill out your copyright notice in the Description page of Project Settings.

#include "SquadUp/MySquadMovementSubsystem.h"
#include "SquadUp/SquadTypes.h"
#include "SquadUp/MySquadSubsystem.h"
#include "Character/BaseCharacter.h"
#include "Character/CharacterAttributeDataAsset.h"
#include "Kismet/GameplayStatics.h"

// 识别 UWorld 指针
#include "Engine/World.h"

// 优化 3：标记为 const
FVector UMySquadMovementSubsystem::GetTacticalLocation(ABaseCharacter* Character) const
{
    if (!Character) return FVector::ZeroVector;

    // 优化 2 核心：不再进行双重 for 循环遍历找人，直接 O(1) 极速查表！
    if (const FVector* CachedLoc = TacticalLocationCache.Find(Character))
    {
        return *CachedLoc;
    }

    // 兜底：如果本帧由于各种原因没算出来他的位置，让他呆在原地
    return Character->GetActorLocation();
}

void UMySquadMovementSubsystem::Tick(float DeltaTime)
{
    UpdateMovementLogic(DeltaTime);
}

void UMySquadMovementSubsystem::UpdateMovementLogic(float DeltaTime)
{
    UWorld* World = GetWorld();
    if (!World) return;

    UMySquadSubsystem* SquadSub = World->GetSubsystem<UMySquadSubsystem>();
    if (!SquadSub) return;

    // --- 优化 1 核心：玩家 Pawn 懒加载缓存 ---
    // 只有在指针失效（比如刚开局，或者玩家死了换角色）时才去执行昂贵的全局检索
    if (!CachedPlayerPawn.IsValid())
    {
        CachedPlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0);
    }

    APawn* Player = CachedPlayerPawn.Get();
    if (!Player) return;

    FVector PlayerLoc = Player->GetActorLocation();

    // 性能秘诀：使用 Reset() 而不是 Empty()！
    // Reset 会清空表里的数据，但保留底层已经开辟的内存容量，下一次循环填入时 0 内存开销！
    TacticalLocationCache.Reset();

    // 一帧只跑一次大遍历，集中处理所有小组的移动与战术坐标分配
    for (auto& Group : SquadSub->GetActiveGroups())
    {
        if (Group.GetValidMemberCount() == 0) continue;

        ABaseCharacter* Representative = Group.GetRepresentative();
        if (!Representative) continue;

        const UCharacterAttributeDataAsset* Config = Representative->GetAttributeConfig();
        if (!Config) continue;

        // 步骤 A：更新整个小组的向导锚点 (AnchorLocation)
        bool bCanMove = (Config->AIDetectionLevel == EAIDetectionLevel::NoPerception || Group.bIsAggro);
        if (bCanMove)
        {
            FVector CurrentCenter = Group.GetGroupCenter();
            FVector DirFromPlayer = (CurrentCenter - PlayerLoc).GetSafeNormal();

            float StopDistance = 600.f;
            FVector FinalTarget = PlayerLoc + DirFromPlayer * StopDistance;

            Group.AnchorLocation = FMath::VInterpTo(Group.AnchorLocation, FinalTarget, DeltaTime, 1.5f);
        }

        // 步骤 B：一口气算完该小队内所有成员的最终战术坐标，并写进黑板（哈希表）
        for (int32 i = 0; i < Group.Members.Num(); ++i)
        {
            ABaseCharacter* Member = Group.GetMemberAtIndex(i);
            if (!Member) continue;

            FVector MemberTacticalLoc = Member->GetActorLocation();

            if (i == 0)
            {
                // 队长占锚点
                MemberTacticalLoc = Group.AnchorLocation;
            }
            else
            {
                // 队员跟在前面的人后面
                if (ABaseCharacter* PrevMember = Group.GetMemberAtIndex(i - 1))
                {
                    MemberTacticalLoc = PrevMember->GetActorLocation() - PrevMember->GetActorForwardVector() * 100.f;
                }
            }

            // 写入缓存表，等待 AI 自己过来取
            TacticalLocationCache.Add(Member, MemberTacticalLoc);
        }
    }
}
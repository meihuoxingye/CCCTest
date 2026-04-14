// Fill out your copyright notice in the Description page of Project Settings.


#include "SquadUp/MySquadSubsystem.h"
#include "Character/BaseCharacter.h"
#include "Character/CharacterAttributeDataAsset.h"
#include "Kismet/GameplayStatics.h"

void UMySquadSubsystem::RegisterCandidate(ABaseCharacter* Character) { if (Character) Candidates.AddUnique(Character); }

void UMySquadSubsystem::UnregisterCandidate(ABaseCharacter* Character)
{
    Candidates.Remove(Character);
    for (int32 i = ActiveGroups.Num() - 1; i >= 0; --i)
    {
        ActiveGroups[i].Members.Remove(Character);
        if (ActiveGroups[i].Members.Num() < 2) ActiveGroups.RemoveAtSwap(i);
    }
}

void UMySquadSubsystem::Tick(float DeltaTime)
{
    UpdateGroupingLogic();
    UpdateMovementLogic(DeltaTime);
}

void UMySquadSubsystem::UpdateGroupingLogic()
{
    // 自动清理无效指针
    Candidates.RemoveAll([](const TWeakObjectPtr<ABaseCharacter>& C) { return !C.IsValid(); });

    for (int32 i = 0; i < Candidates.Num(); ++i)
    {
        ABaseCharacter* A = Candidates[i].Get();
        if (!A) continue;

        TArray<ABaseCharacter*> Found;
        Found.Add(A);

        for (int32 j = i + 1; j < Candidates.Num(); ++j)
        {
            ABaseCharacter* B = Candidates[j].Get();
            // 组队条件：同类型(Friendly/Enemy)、在感应范围内
            if (B && A->GetAttributeConfig()->CharacterType == B->GetAttributeConfig()->CharacterType)
            {
                if (FVector::Dist(A->GetActorLocation(), B->GetActorLocation()) < A->GetAttributeConfig()->GroupingRadius)
                {
                    Found.Add(B);
                    if (Found.Num() >= FMath::RandRange(2, 4)) break;
                }
            }
        }

        if (Found.Num() >= 2)
        {
            FSquadGroup NewGroup;
            for (auto* M : Found) { NewGroup.Members.Add(M); Candidates.Remove(M); }

            // 给每个小组一个随机的初始时间偏移，让它们的 Y 轴上下移动节奏错开
            NewGroup.YTimer = FMath::RandRange(0.f, 6.28f);

            ActiveGroups.Add(NewGroup);
            --i;
        }
    }
}

void UMySquadSubsystem::UpdateMovementLogic(float DeltaTime)
{
    APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
    if (!Player) return;

    // 1. 基础移动逻辑计算
    for (auto& Group : ActiveGroups)
    {
        FVector SumPos = FVector::ZeroVector;
        int32 Count = 0;
        for (auto& M : Group.Members) if (M.IsValid()) { SumPos += M->GetActorLocation(); Count++; }
        if (Count == 0) continue;

        FVector Center = SumPos / Count;
        FVector DirToPlayer = (Player->GetActorLocation() - Center).GetSafeNormal();
        float Dist = FVector::Dist(Center, Player->GetActorLocation());

        FVector Base = Center;
        if (Dist > 800.f) Base += DirToPlayer * 100.f;
        else if (Dist < 400.f) Base -= DirToPlayer * 150.f;

        // --- 新增：组间斥力逻辑 ---
        FVector RepulsionForce = FVector::ZeroVector;
        float GroupAvoidanceRadius = 400.f; // 组与组之间的安全距离

        for (const auto& OtherGroup : ActiveGroups)
        {
            // 不要和自己排斥
            if (&Group == &OtherGroup) continue;

            float GroupDist = FVector::Dist(Group.AnchorLocation, OtherGroup.AnchorLocation);
            if (GroupDist < GroupAvoidanceRadius && GroupDist > 0.1f)
            {
                // 计算排斥方向：从“他人”指向“自己”
                FVector PushDir = (Group.AnchorLocation - OtherGroup.AnchorLocation).GetSafeNormal();
                // 距离越近，推力越大
                RepulsionForce += PushDir * (GroupAvoidanceRadius - GroupDist);
            }
        }
        Base += RepulsionForce; // 应用斥力，将小组中心推开
        // -----------------------

        Group.YTimer += DeltaTime * 2.0f;
        Group.AnchorLocation = FVector(Base.X, Player->GetActorLocation().Y + FMath::Sin(Group.YTimer) * 200.f, Base.Z);
    }
}

FVector UMySquadSubsystem::GetTacticalLocation(ABaseCharacter* Character)
{
    for (auto& Group : ActiveGroups)
    {
        int32 Idx = Group.Members.Find(Character);
        if (Idx != INDEX_NONE)
        {
            // 调大 X 和 Y 的间距（例如从 100 调到 150）
            FVector FormationOffset = FVector(Idx * -150.f, Idx * 100.f, 0.f);

            // 增加一点随机扰动，防止过于僵硬
            FVector RandomJitter = FVector(FMath::RandRange(-30.f, 30.f), FMath::RandRange(-30.f, 30.f), 0.f);

            return Group.AnchorLocation + FormationOffset + RandomJitter;
        }
    }
    return Character->GetActorLocation();
}
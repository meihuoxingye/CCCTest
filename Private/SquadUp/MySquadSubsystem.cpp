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
            ActiveGroups.Add(NewGroup);
            --i;
        }
    }
}

void UMySquadSubsystem::UpdateMovementLogic(float DeltaTime)
{
    APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
    if (!Player) return;

    for (auto& Group : ActiveGroups)
    {
        FVector SumPos = FVector::ZeroVector;
        int32 Count = 0;
        for (auto& M : Group.Members) if (M.IsValid()) { SumPos += M->GetActorLocation(); Count++; }
        if (Count == 0) continue;

        FVector Center = SumPos / Count;
        FVector DirToPlayer = (Player->GetActorLocation() - Center).GetSafeNormal();
        float Dist = FVector::Dist(Center, Player->GetActorLocation());

        // 战术位移
        FVector Base = Center;
        if (Dist > 800.f) Base += DirToPlayer * 100.f;
        else if (Dist < 400.f) Base -= DirToPlayer * 150.f;

        // Y轴波动走位
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
            // 每个成员根据索引获得偏移，且带有随机抖动
            return Group.AnchorLocation + FVector(Idx * -100.f, Idx * 50.f, 0.f) + FVector(FMath::RandRange(-20.f, 20.f));
        }
    }
    return Character->GetActorLocation();
}
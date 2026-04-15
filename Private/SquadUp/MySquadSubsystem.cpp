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

    FVector PlayerLoc = Player->GetActorLocation();
    FVector PlayerForward = Player->GetActorForwardVector(); // 以角色为顶点的锥形方向

    for (auto& Group : ActiveGroups)
    {
        // 1. 计算小组当前重心
        FVector Center = GetGroupCenter(Group); // 提取出的计算重心函数
        FVector DirToGroup = (Center - PlayerLoc).GetSafeNormal();

        // 2. 锥形范围判定 (使用点积)
        // 假设锥形半角为 45 度，cos(45) ≈ 0.707
        float DotValue = FVector::DotProduct(PlayerForward, DirToGroup);
        bool bInCone = DotValue > 0.707f;

        // 3. 检查小组成员是否正在射击 (射击时停止移动)
        /*
        bool bAnyMemberShooting = false;
        for (auto& M : Group.Members)
        {
            if (ABaseCharacter* BC = Cast<ABaseCharacter>(M.Get()))
            {
                // 这里需要你在 CombatComponent 里加一个 IsShooting() 接口
                // 或者通过检测是否在 FiringSubsystem 的名单里
                if (BC->GetCombatComponent()->IsCurrentlyFiring())
                {
                    bAnyMemberShooting = true;
                    break;
                }
            }
        }
        */

        // 4. 状态机驱动位移
        /*
        if (bAnyMemberShooting)
        {
            // 射击时：锁定锚点不动
            continue;
        }
        */

        if (!bInCone)
        {
            // 范围外：主动进入锥形范围
            // 计算目标：玩家前方的一个点
            FVector TargetPoint = PlayerLoc + (PlayerForward * 600.f);
            Group.AnchorLocation = FMath::VInterpTo(Group.AnchorLocation, TargetPoint, DeltaTime, 1.5f);
        }
        else
        {
            // 范围内：缓慢上下来回移动 (一会动一会不动可以通过计时器实现)
            Group.YTimer += DeltaTime * 1.0f; // 降低频率
            float YWave = FMath::Sin(Group.YTimer) * 150.f;

            // 缓慢向玩家平移，同时保持 Y 轴震荡
            FVector SlowMove = FMath::VInterpTo(Group.AnchorLocation, PlayerLoc + (DirToGroup * 400.f), DeltaTime, 0.5f);
            Group.AnchorLocation = FVector(SlowMove.X, SlowMove.Y + YWave, SlowMove.Z);
        }
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

FVector UMySquadSubsystem::GetGroupCenter(const FSquadGroup& Group) const
{
    FVector SumLocation = FVector::ZeroVector;
    int32 ValidCount = 0;

    for (const auto& MemberPtr : Group.Members)
    {
        if (ABaseCharacter* Member = MemberPtr.Get())
        {
            SumLocation += Member->GetActorLocation();
            ValidCount++;
        }
    }

    // 防止除以 0（万一小组里没人了）
    return (ValidCount > 0) ? (SumLocation / (float)ValidCount) : FVector::ZeroVector;
}
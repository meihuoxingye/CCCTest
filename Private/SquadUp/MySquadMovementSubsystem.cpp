// Fill out your copyright notice in the Description page of Project Settings.


#include "SquadUp/MySquadMovementSubsystem.h"
#include "SquadUp/SquadTypes.h"
#include "SquadUp/MySquadSubsystem.h"

FVector UMySquadMovementSubsystem::GetTacticalLocation(ABaseCharacter* Character)
{
    for (auto& Group : ActiveGroups)
    {
        int32 Idx = Group.Members.Find(Character);
        if (Idx == INDEX_NONE) continue;

        // 0号领队：前往小队锚点
        if (Idx == 0) return Group.AnchorLocation;

        // ✅ 修复：衔尾蛇跟随逻辑。后一个人跟在前一个人的正后方 1 米
        if (Group.Members[Idx - 1].IsValid())
        {
            ABaseCharacter* PrevMember = Group.Members[Idx - 1].Get();
            // 目标点 = 前一人的位置 - 前一人的朝向向量 * 100单位(1米)
            return PrevMember->GetActorLocation() - PrevMember->GetActorForwardVector() * 100.f;
        }
    }
    return Character->GetActorLocation();
}

void UMySquadMovementSubsystem::Tick(float DeltaTime)
{
    UpdateMovementLogic(DeltaTime);
}

void UMySquadMovementSubsystem::UpdateMovementLogic(float DeltaTime)
{
    APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
    if (!Player) return;
    FVector PlayerLoc = Player->GetActorLocation();

    for (auto& Group : ActiveGroups)
    {
        if (Group.Members.Num() == 0) continue;

        // 获取小队代表判断觉察状态
        ABaseCharacter* Representative = nullptr;
        for (auto& M : Group.Members) { if (M.IsValid()) { Representative = M.Get(); break; } }
        if (!Representative) continue;

        // 仅在察觉或进入仇恨状态后移动
        bool bCanMove = (Representative->GetAttributeConfig()->AIDetectionLevel == EAIDetectionLevel::NoPerception || Group.bIsAggro);
        if (!bCanMove) continue;

        // 计算小队重心到玩家的方向
        FVector CurrentCenter = GetGroupCenter(Group);
        FVector DirFromPlayer = (CurrentCenter - PlayerLoc).GetSafeNormal();

        // 设置跟踪停止距离（例如保持在 6 米外，可根据需要调整）
        float StopDistance = 600.f;
        FVector FinalTarget = PlayerLoc + DirFromPlayer * StopDistance;

        // 锚点直接向目标插值，带动整支队伍
        Group.AnchorLocation = FMath::VInterpTo(Group.AnchorLocation, FinalTarget, DeltaTime, 1.5f);
    }
}

FVector UMySquadMovementSubsystem::GetGroupCenter(const FSquadGroup& Group) const
{
    FVector Sum = FVector::ZeroVector;
    int32 Count = 0;
    for (const auto& M : Group.Members) { if (M.IsValid()) { Sum += M->GetActorLocation(); Count++; } }

    // 防止除以 0（万一小组里没人了）
    return (Count > 0) ? (Sum / (float)Count) : FVector::ZeroVector;
}
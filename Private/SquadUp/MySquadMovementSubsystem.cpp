// Fill out your copyright notice in the Description page of Project Settings.


#include "SquadUp/MySquadMovementSubsystem.h"
#include "SquadUp/SquadTypes.h"
#include "SquadUp/MySquadSubsystem.h"
#include "Character/BaseCharacter.h"
#include "Character/CharacterAttributeDataAsset.h"
#include "Kismet/GameplayStatics.h"

FVector UMySquadMovementSubsystem::GetTacticalLocation(ABaseCharacter* Character)
{
    // 1. 角色判空（防止 Character->GetActorLocation() 崩溃）
    if (!Character) return FVector::ZeroVector;

    // 2. 世界指针判空
    UWorld* World = GetWorld();
    if (!World) return Character->GetActorLocation();

    // 3. 子系统判空
    UMySquadSubsystem* SquadSub = World->GetSubsystem<UMySquadSubsystem>();
    if (!SquadSub) return Character->GetActorLocation();

    // 4. 执行逻辑（使用你已经写好的安全方法）
    for (auto& Group : SquadSub->GetActiveGroups())
    {
        int32 Idx = Group.GetMemberIndex(Character);
        if (Idx == INDEX_NONE) continue;

        if (Idx == 0) return Group.AnchorLocation;

        if (ABaseCharacter* PrevMember = Group.GetMemberAtIndex(Idx - 1))
        {
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
    UWorld* World = GetWorld();
    if (!World) return;

    UMySquadSubsystem* SquadSub = World->GetSubsystem<UMySquadSubsystem>();
    if (!SquadSub) return;

    APawn* Player = UGameplayStatics::GetPlayerPawn(World, 0);
    if (!Player) return;
    FVector PlayerLoc = Player->GetActorLocation();

    for (auto& Group : SquadSub->GetActiveGroups())
    {
        if (Group.GetValidMemberCount() == 0) continue;

        ABaseCharacter* Representative = Group.GetRepresentative();
        if (!Representative) continue;

        const UCharacterAttributeDataAsset* Config = Representative->GetAttributeConfig();
        // 0xbf8 崩溃最可能的发生地：如果 Config 为空，下一行访问 AIDetectionLevel 必崩
        if (!Config) continue;

        bool bCanMove = (Config->AIDetectionLevel == EAIDetectionLevel::NoPerception || Group.bIsAggro);
        if (!bCanMove) continue;

        FVector CurrentCenter = Group.GetGroupCenter();
        FVector DirFromPlayer = (CurrentCenter - PlayerLoc).GetSafeNormal();

        float StopDistance = 600.f;
        FVector FinalTarget = PlayerLoc + DirFromPlayer * StopDistance;

        Group.AnchorLocation = FMath::VInterpTo(Group.AnchorLocation, FinalTarget, DeltaTime, 1.5f);
    }
}
// Fill out your copyright notice in the Description page of Project Settings.


#include "SkillSystem/SkillPointSubsystem.h"

void USkillPointSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    FCharacterSPData WarriorData;
    WarriorData.LastSyncGameTime = 0.0;
    SquadSPMap.Add(TEXT("Hero_Warrior"), WarriorData);
}

// 优化：标记为 const 函数，且使用 Find() 实现单次哈希查找
float USkillPointSubsystem::GetCharacterSP(FName CharacterID) const
{
    // 在 const 函数中，TMap::Find 会自动返回 const 指针，完美契合只读安全
    const FCharacterSPData* DataPtr = SquadSPMap.Find(CharacterID);
    if (!DataPtr) return 0.0f;

    if (DataPtr->bIsRegenFrozen || !GetWorld())
    {
        return DataPtr->SavedSP;
    }

    double CurrentGameTime = GetWorld()->GetTimeSeconds();
    double TimePassed = CurrentGameTime - DataPtr->LastSyncGameTime;

    if (TimePassed < 0.0) TimePassed = 0.0;

    float CalculatedSP = DataPtr->SavedSP + (static_cast<float>(TimePassed) * DataPtr->RegenRate);
    return FMath::Clamp(CalculatedSP, 0.0f, DataPtr->MaxSP);
}

// 优化：标记为 const 函数
int32 USkillPointSubsystem::GetCurrentSPAsInt(FName CharacterID) const
{
    return FMath::FloorToInt(GetCharacterSP(CharacterID));
}

bool USkillPointSubsystem::ConsumeCharacterSP(FName CharacterID, float Amount)
{
    // 非 const 函数，使用 Find() 获取可写指针
    FCharacterSPData* DataPtr = SquadSPMap.Find(CharacterID);
    if (!DataPtr) return false;

    // GetCharacterSP 是 const 的，在这里安全调用
    float CurrentSP = GetCharacterSP(CharacterID);

    if (CurrentSP >= Amount)
    {
        DataPtr->SavedSP = CurrentSP - Amount;
        DataPtr->LastSyncGameTime = GetWorld()->GetTimeSeconds();
        return true;
    }
    return false;
}

void USkillPointSubsystem::SetCharacterRegenFrozen(FName CharacterID, bool bFreeze)
{
    FCharacterSPData* DataPtr = SquadSPMap.Find(CharacterID);
    if (!DataPtr || DataPtr->bIsRegenFrozen == bFreeze) return;

    if (bFreeze)
    {
        // 冻结：利用单次查找拿到的指针直接固化数据
        DataPtr->SavedSP = GetCharacterSP(CharacterID);
        DataPtr->bIsRegenFrozen = true;
    }
    else
    {
        // 解冻：对齐时间轴
        DataPtr->LastSyncGameTime = GetWorld()->GetTimeSeconds();
        DataPtr->bIsRegenFrozen = false;
    }
}

void USkillPointSubsystem::SetAllCharactersRegenFrozen(bool bFreeze)
{
    TArray<FName> Keys;
    SquadSPMap.GetKeys(Keys);
    for (const FName& Key : Keys)
    {
        SetCharacterRegenFrozen(Key, bFreeze);
    }
}

void USkillPointSubsystem::PrepareForSave()
{
    TArray<FName> Keys;
    SquadSPMap.GetKeys(Keys);
    for (const FName& Key : Keys)
    {
        SetCharacterRegenFrozen(Key, true);
    }
}

void USkillPointSubsystem::PostLoadSync()
{
    TArray<FName> Keys;
    SquadSPMap.GetKeys(Keys);
    for (const FName& Key : Keys)
    {
        FCharacterSPData* DataPtr = SquadSPMap.Find(Key);
        if (DataPtr)
        {
            DataPtr->LastSyncGameTime = GetWorld()->GetTimeSeconds();
            DataPtr->bIsRegenFrozen = false;
        }
    }
}
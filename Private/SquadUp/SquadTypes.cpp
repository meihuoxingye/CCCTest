// Fill out your copyright notice in the Description page of Project Settings.


#include "SquadUp/SquadTypes.h"
#include "Character/BaseCharacter.h" // 必须包含此头文件才能调用角色的函数

bool FSquadGroup::IsFull() const { return Members.Num() >= FixedMaxCapacity; }

int32 FSquadGroup::GetValidMemberCount() const
{
    int32 Count = 0;
    for (const auto& M : Members) { if (M.IsValid()) Count++; }
    return Count;
}

bool FSquadGroup::TryAddMember(ABaseCharacter* NewMember)
{
    if (NewMember && !IsFull()) {
        Members.AddUnique(NewMember);
        return true;
    }
    return false;
}

FVector FSquadGroup::GetGroupCenter() const
{
    FVector Sum = FVector::ZeroVector;
    int32 Count = 0;
    for (const auto& M : Members) {
        if (ABaseCharacter* C = M.Get()) { // 弱指针通过 Get() 转换
            Sum += C->GetActorLocation();
            Count++;
        }
    }
    return (Count > 0) ? (Sum / (float)Count) : FVector::ZeroVector;
}

int32 FSquadGroup::GetMemberIndex(ABaseCharacter* Character) const
{
    for (int32 i = 0; i < Members.Num(); ++i) {
        if (Members[i].Get() == Character) return i;
    }
    return INDEX_NONE;
}

ABaseCharacter* FSquadGroup::GetMemberAtIndex(int32 Index) const
{
    return Members.IsValidIndex(Index) ? Members[Index].Get() : nullptr;
}

ABaseCharacter* FSquadGroup::GetRepresentative() const
{
    for (const auto& M : Members) { if (M.IsValid()) return M.Get(); }
    return nullptr;
}
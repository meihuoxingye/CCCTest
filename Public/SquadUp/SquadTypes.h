// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SquadTypes.generated.h" // 确保有这一行！

// 一个小组的数据结构
USTRUCT()
struct FSquadGroup
{
    GENERATED_BODY()

    // 使用弱指针，当角色被销毁了，弱指针会自动意识到“目标已消失”
    // 可用于 RemoveAll 的逻辑判断
    UPROPERTY()
    TArray<TWeakObjectPtr<class ABaseCharacter>> Members;
    TWeakObjectPtr<class ABaseCharacter> Captain;

    // 本小组的锚点位置，组队搜索的圆形位置
    FVector AnchorLocation;

    // 该小队永久固定的成员上限
    UPROPERTY()
    int32 FixedMaxCapacity = 2;

    // 标记该小组是否已进入战斗状态（发现目标）
    bool bIsAggro = false;


    // --- 新增：数据自管理方法 ---

    // 检查是否满员
    bool IsFull() const;

    // 获取有效成员数量（排除死掉的）
    int32 GetValidMemberCount() const;

    // 若小组未满员，则从 found池 加入新成员
    bool TryAddMember(ABaseCharacter* NewMember);


    // ==========================================
    // 新增：小队查询与数学计算方法
    // ==========================================

    // 获取某个角色在队伍里的索引
    int32 GetMemberIndex(class ABaseCharacter* Character) const;

    // 安全地获取指定索引的队员
    ABaseCharacter* GetMemberAtIndex(int32 Index) const;

    // 找一个活着的代表（用于判断仇恨/感知状态）
    ABaseCharacter* GetRepresentative() const;

    // 计算本小队的几何重心
    FVector GetGroupCenter() const;
};
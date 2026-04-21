// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

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
};
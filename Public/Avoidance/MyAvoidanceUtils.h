// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MyAvoidanceUtils.generated.h"

/**
 * 
 */
UCLASS()
class CCC_API UMyAvoidanceUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
    /**
         * 核心避障算法：仿虚幻人群避障的自适应采样逻辑
         * @param CurrentLoc 当前位置
         * @param CurrentVel 当前速度
         * @param DesiredVel 期望速度（指向战术目标的理想向量）
         * @param MaxSpeed 最大移速
         * @param Neighbors 附近需要避让的单位
         * @return 经过 RVO 修正后的最优速度向量
         */
    static FVector CalculateAvoidanceVelocity(
        const FVector& CurrentLoc,
        const FVector& CurrentVel,
        const FVector& DesiredVel,
        float MaxSpeed,
        const TArray<TWeakObjectPtr<class ABaseCharacter>>& Neighbors
    );

private:
    // 计算碰撞预警时间 (Time To Collision)
    static float GetTimeToCollision(const FVector& RelPos, const FVector& RelVel, float Radius);
};

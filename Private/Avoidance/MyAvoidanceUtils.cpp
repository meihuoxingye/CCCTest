// Fill out your copyright notice in the Description page of Project Settings.


#include "Avoidance/MyAvoidanceUtils.h"
#include "Character/BaseCharacter.h"

FVector UMyAvoidanceUtils::CalculateAvoidanceVelocity(const FVector& CurrentLoc, const FVector& CurrentVel, const FVector& DesiredVel, float MaxSpeed, const TArray<TWeakObjectPtr<class ABaseCharacter>>& Neighbors)
{
    if (DesiredVel.IsNearlyZero()) return FVector::ZeroVector;

    FVector BestVelocity = DesiredVel;
    float BestScore = -MAX_FLT;

    // 自适应采样：在期望速度周围生成候选点
    const int32 NumSamples = 12;
    for (int32 i = 0; i < NumSamples; ++i)
    {
        // 旋转期望向量产生采样点
        FVector SampleDir = DesiredVel.RotateAngleAxis(FMath::FRandRange(-45.f, 45.f), FVector::UpVector);
        FVector CandidateVel = SampleDir * FMath::FRandRange(0.5f, 1.0f);

        // --- 评价该采样的风险 ---
        float CollisionRisk = 0.f;
        for (const auto& Neighbor : Neighbors)
        {
            if (!Neighbor.IsValid()) continue;

            FVector RelPos = Neighbor->GetActorLocation() - CurrentLoc;
            FVector RelVel = CandidateVel - Neighbor->GetVelocity();

            // 预测未来碰撞时间 (Radius 设为两个单位半径之和，约100)
            float TTC = GetTimeToCollision(RelPos, RelVel, 100.f);
            if (TTC > 0.f && TTC < 2.0f) // 只关注 2 秒内的碰撞风险
            {
                CollisionRisk += (2.0f - TTC); // 撞得越快，扣分越多
            }
        }

        // 得分 = 进度得分（点积） - 风险惩罚
        float ProgressScore = FVector::DotProduct(CandidateVel.GetSafeNormal(), DesiredVel.GetSafeNormal());
        float TotalScore = ProgressScore - (CollisionRisk * 3.0f);

        if (TotalScore > BestScore)
        {
            BestScore = TotalScore;
            BestVelocity = CandidateVel;
        }
    }

    // 应用 50% RVO 责任修正，防止抖动
    return CurrentVel + (BestVelocity - CurrentVel) * 0.5f;
}

float UMyAvoidanceUtils::GetTimeToCollision(const FVector& RelPos, const FVector& RelVel, float Radius)
{
    float a = FVector::DotProduct(RelVel, RelVel);
    if (a < KINDA_SMALL_NUMBER) return -1.f;

    float b = 2.0f * FVector::DotProduct(RelPos, RelVel);
    float c = FVector::DotProduct(RelPos, RelPos) - (Radius * Radius);

    float Discriminant = b * b - 4.0f * a * c;
    if (Discriminant < 0) return -1.f;

    float t = (-b - FMath::Sqrt(Discriminant)) / (2.0f * a);
    return (t > 0) ? t : -1.f;
}

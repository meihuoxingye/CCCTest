// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon/AsyncLineTraceBullet/MyBulletSubsystem.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"

void UMyBulletSubsystem::FireBullet(AActor* InOwner, FVector StartLoc, FVector Direction, float Speed, float LifeTime)
{
    FVirtualBulletData NewBullet;
    NewBullet.Position = StartLoc;
    NewBullet.Velocity = Direction * Speed;
    NewBullet.RemainingLife = LifeTime;
    NewBullet.Owner = InOwner;

    ActiveBullets.Add(NewBullet);
}

void UMyBulletSubsystem::Tick(float DeltaTime)
{
    if (ActiveBullets.Num() == 0) return;

    Accumulator += DeltaTime;

    // 性能优化策略：如果累积时间没到 1/30 秒，就不更新物理，直接跳过
    if (Accumulator < TargetUpdateInterval) return;

    const float SubstepTime = Accumulator; // 实际跨越的时间
    Accumulator = 0.f;

    // 倒序遍历数组，方便在循环内删除过期子弹
    for (int32 i = ActiveBullets.Num() - 1; i >= 0; --i)
    {
        FVirtualBulletData& Bullet = ActiveBullets[i];

        // 计算这一段位移
        FVector NextLoc = Bullet.Position + (Bullet.Velocity * SubstepTime);

        FHitResult Hit;
        FCollisionQueryParams Params;
        Params.AddIgnoredActor(Bullet.Owner);

        // 分段线迹追踪
        if (GetWorld()->LineTraceSingleByChannel(Hit, Bullet.Position, NextLoc, ECC_Visibility, Params))
        {
            // 命中逻辑
            if (Hit.GetActor())
            {
                // 这里调用你的伤害接口，例如：
                // UGameplayStatics::ApplyDamage(Hit.GetActor(), 20.f, ...);
            }

            // 绘制命中点（调试用）
            DrawDebugSphere(GetWorld(), Hit.ImpactPoint, 10.f, 8, FColor::Red, false, 0.5f);

            ActiveBullets.RemoveAtSwap(i); // 使用 RemoveAtSwap 进一步提升大数组删除效率
            continue;
        }

        // 没撞到，更新位置并画出“肉眼可见”的轨迹
        // 这里的 0.05f 持续时间略大于 30Hz 间隔，保证视觉连贯无闪烁
        DrawDebugLine(GetWorld(), Bullet.Position, NextLoc, FColor::Yellow, false, 0.05f, 0, 1.0f);

        Bullet.Position = NextLoc;
        Bullet.RemainingLife -= SubstepTime;

        if (Bullet.RemainingLife <= 0.f)
        {
            ActiveBullets.RemoveAtSwap(i);
        }
    }
}
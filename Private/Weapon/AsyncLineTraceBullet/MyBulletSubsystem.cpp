// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon/AsyncLineTraceBullet/MyBulletSubsystem.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"

void UMyBulletSubsystem::FireBullet(AActor* InOwner, const FVector& StartLoc, const FVector& Direction, float Speed, float LifeTime)
{
    // 定义一个新的子弹数据结构体并设置参数
    FVirtualBulletData NewBullet;
    NewBullet.Owner = InOwner;
    NewBullet.Position = StartLoc;
    NewBullet.Velocity = Direction * Speed;
    NewBullet.RemainingLife = LifeTime;
    
    // 将结构体添加到子弹池
    ActiveBullets.Add(NewBullet);
}

void UMyBulletSubsystem::Tick(float DeltaTime)
{
    // 如果子弹池为空，则跳过更新，节省性能
    if (ActiveBullets.Num() == 0) return;

    // 累积时间
    Accumulator += DeltaTime;

    // 性能优化策略：用 while 循环！只要累积时间超过 1/30，就按标准的 1/30 切割并执行
    // 
    while (Accumulator >= TargetUpdateInterval)
    {
        // 每次只消耗掉 1/30 秒，没用完的时间留在 Accumulator 里等下一帧！
        Accumulator -= TargetUpdateInterval;

        // 永远固定的步长时间
        const float FixedStep = TargetUpdateInterval;

        // 倒序遍历数组，处理这段时间内的所有子弹
        // 为什么倒序，因为可能会因为中途撞墙，在循环结束前提前摧毁子弹
        // 正序删除会导致数组元素前移，漏掉一个子弹不处理；倒序删除则不会有这个问题
        for (int32 i = ActiveBullets.Num() - 1; i >= 0; --i)
        {
            FVirtualBulletData& Bullet = ActiveBullets[i];

            // 计算子弹在这段时间内的下一位置
            // 每次最多只往前飞 1/30 秒的距离，绝不会因为卡顿而一次性瞬移飞出地图
            const FVector NextLoc = Bullet.Position + (Bullet.Velocity * FixedStep);

            // 碰撞/命中调查报告
            FHitResult Hit;
            // FCollisionQueryParams 是虚幻引擎提供的一个结构体，相当于一份碰撞规则调查问卷或过滤清单
            // 这份清单里可以配置很多高级选项，比如：是否要检测复杂碰撞（逐多边形检测）、是否返回物理材质（打中木头还是铁）、以及要忽略哪些物体
            FCollisionQueryParams Params;
            // 把当前这颗子弹的主人加入忽略列表
            Params.AddIgnoredActor(Bullet.Owner);

            // 这是一个纯正的同步（Synchronous）阻塞函数
            // 异步（解耦）是指结构，主人与子弹开火解耦与生命周期分离
            // 有真正的异步射线检测函数且性能更好，但也有响应的问题且更复杂
            if (GetWorld()->LineTraceSingleByChannel(Hit, Bullet.Position, NextLoc, ECC_Visibility, Params))
            {
                // 射线命中逻辑
                if (Hit.GetActor())
                {
                    // 这里调用你的伤害接口，例如：
                    // UGameplayStatics::ApplyDamage(Hit.GetActor(), 20.f, ...);
                }

                // 绘制命中点（调试用）
                DrawDebugSphere(GetWorld(), Hit.ImpactPoint, 10.f, 8, FColor::Red, false, 0.5f);

                // 使用 RemoveAtSwap 进一步提升大数组删除效率
                ActiveBullets.RemoveAtSwap(i); 
                // 重新 for 循环，直到处理完所有射线命中的子弹
                continue;
            }

            // 对于射线没撞到实体的子弹，更新位置并画出“肉眼可见”的轨迹
            // 这里的 0.05f 持续时间略大于 30Hz 间隔，保证视觉连贯无闪烁
            DrawDebugLine(GetWorld(), Bullet.Position, NextLoc, FColor::Yellow, false, 0.05f, 0, 1.0f);

            Bullet.Position = NextLoc;
            // 子弹生命周期自减一次线迹追踪间隔时间
            Bullet.RemainingLife -= FixedStep;

            // 子弹生命周期小于0则销毁
            if (Bullet.RemainingLife <= 0.f)
            {
                ActiveBullets.RemoveAtSwap(i);
            }
        }
    }
}
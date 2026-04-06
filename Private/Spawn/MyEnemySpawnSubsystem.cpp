// Fill out your copyright notice in the Description page of Project Settings.


#include "Spawn/MyEnemySpawnSubsystem.h"

#include "Spawn/MyEnemySpawnConfig.h"

#include "TimerManager.h"

void UMyEnemySpawnSubsystem::RegisterSpawnPoint(USceneComponent* Point)
{
    if (Point) SpawnPoints.AddUnique(Point);
}

void UMyEnemySpawnSubsystem::UnregisterSpawnPoint(USceneComponent* Point)
{
    SpawnPoints.Remove(Point);
}

void UMyEnemySpawnSubsystem::StartSpawningWithConfig(UMyEnemySpawnConfig* Config)
{
    if (!Config || !Config->EnemyClass) return;

    CurrentConfig = Config;
    RemainingCount = Config->TotalSpawnCount;

    // 启动循环计时器
    GetWorld()->GetTimerManager().SetTimer(
        SpawnTimerHandle,
        this,
        // 时间到了，就执行函数 ExecuteSpawn
        &UMyEnemySpawnSubsystem::ExecuteSpawn,
        Config->Interval,
        true,
        // 立即执行一次，后续每隔 Interval 秒执行一次
        // 增加一个小的初始延迟，避免刷怪和场景加载的性能冲突
        0.1f
    );
}

void UMyEnemySpawnSubsystem::ExecuteSpawn()
{
    // 清理已经被销毁的组件指针
    // 由于使用了 TWeakObjectPtr，组件被销毁后指针会自动变为无效，只需要定期清理掉这些无效的指针即可
    // Lambda 表达式，[] 为输入的外部变量，RemoveAll 遍历数组传给 P，执行判断，为 true 则删除
    SpawnPoints.RemoveAll([](const TWeakObjectPtr<USceneComponent>& P) { return !P.IsValid(); });

    // 终止条件，当剩余生成数量为 0，或者没有任何生成点了，或者配置被意外销毁了（例如关卡重置时），都要停止计时器
    if (RemainingCount <= 0 || SpawnPoints.Num() == 0 || !CurrentConfig)
    {
        GetWorld()->GetTimerManager().ClearTimer(SpawnTimerHandle);
        return;
    }

    // SpawnActor 的生成参数表
    FActorSpawnParameters Params;
    // 碰撞处理覆盖设置：尽量调整位置，但无论如何都要生出来
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;


    // 为 true，从所有点里随机抽一个生
    if (CurrentConfig->bIsRandom)
    {
        int32 Index = FMath::RandRange(0, SpawnPoints.Num() - 1);
        if (USceneComponent* Point = SpawnPoints[Index].Get())
        {
            // 使用生成点的组件的位置和旋转来生成
            GetWorld()->SpawnActor<APawn>(
                CurrentConfig->EnemyClass,
                Point->GetComponentLocation(),
                Point->GetComponentRotation(),
                Params
            );
            // 总共生成几只怪减一
            RemainingCount--;
        }
    }

    // 为 false，在所有已注册的点上同时生
    else
    {
        // 使用 auto 避免写冗长的类型名
        // 遍历所有生成点
        for (auto& PointPtr : SpawnPoints)
        {
            if (USceneComponent* Point = PointPtr.Get())
            {
                GetWorld()->SpawnActor<APawn>(
                    CurrentConfig->EnemyClass,
                    Point->GetComponentLocation(),
                    Point->GetComponentRotation(),
                    Params
                );
            }
        }
        RemainingCount--;
    }
}
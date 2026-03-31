// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MyBulletSubsystem.generated.h"


/**
UWorldSubsystem 是用来实现管理器模式的 C++ 类
既然子弹不再是独立的 AActor 实体
它们就变成了子系统里的一串数据（FVector 坐标、FVector 速度）
既然子弹自己不是 Actor，没法自己动，那就必须有一个管理器来统一处理
管理器被实现为单例，具有全局访问、数据唯一性、内存控制、批量处理的功能

在这里，它的优点：
比 Actor 轻（只是一个内存对象）
全局好找（任何枪都能通过它发射子弹）
自动负责（随地图加载而产生，不用你手动往场景里拖）
自动生命周期
*/

/**
FTickableGameObject 是一个抽象接口类
它的唯一作用是把这个类注册到虚幻引擎的 Tick Manager（每帧任务列表）中
UWorldSubsystem 只是一个内存对象，没有帧更新功能，所以需要 FTickableGameObject
*/

/**
Subsystem + Tickable 几乎是虚幻里最轻量级的方案
*/


// 极其精简的子弹结构体，只占 32 字节，CPU 缓存友好
USTRUCT()
struct FVirtualBulletData
{
    GENERATED_BODY()

    UPROPERTY()
    FVector Position;

    UPROPERTY()
    FVector Velocity;

    UPROPERTY()
    float RemainingLife;

    // 记录是谁打的，防止自伤
    UPROPERTY()
    TObjectPtr<AActor> Owner;
};

UCLASS()
class CCC_API UMyBulletSubsystem : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()
	
public:
    // 外部调用入口：发射子弹
    void FireBullet(AActor* InOwner, FVector StartLoc, FVector Direction, float Speed, float LifeTime);

    // --- FTickableGameObject 接口 ---
    virtual void Tick(float DeltaTime) override;
    virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UMyBulletSubsystem, STATGROUP_Tickables); }
    // -------------------------------

private:
    TArray<FVirtualBulletData> ActiveBullets;

    float Accumulator = 0.f;
    const float TargetUpdateInterval = 1.f / 30.f; // 30Hz 更新，即便游戏 60 帧，性能再翻倍
};


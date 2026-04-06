// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MyEnemySpawnSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class CCC_API UMyEnemySpawnSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
    // USceneComponent 是虚幻引擎中拥有坐标和旋转的最基础基类
    // 子系统只需要知道“在哪生、朝哪看”
    // 外部调用，注册生成点
    void RegisterSpawnPoint(USceneComponent* Point);
    // 外部调用，注销生成点
    void UnregisterSpawnPoint(USceneComponent* Point);

    // 传入数据资产配置，确定刷怪次数间隔，启动刷怪计时器
    UFUNCTION(BlueprintCallable, Category = "Logic")
    void StartSpawningWithConfig(UMyEnemySpawnConfig* Config);

protected:
    // 执行刷怪逻辑
    void ExecuteSpawn();

private:
    // 弱引用数组只存极其轻量的组件指针
    // 存放外部调用注册的生成点
    UPROPERTY()
    TArray<TWeakObjectPtr<USceneComponent>> SpawnPoints;

    UPROPERTY()
    TObjectPtr<UMyEnemySpawnConfig> CurrentConfig;

    // 总共生成几只怪
    int32 RemainingCount = 0;
    // 计时器句柄
    FTimerHandle SpawnTimerHandle;
};

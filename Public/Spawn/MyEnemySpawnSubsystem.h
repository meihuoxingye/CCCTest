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
    void RegisterSpawnPoint(AActor* Point);
    void UnregisterSpawnPoint(AActor* Point);

    // 改为传入数据资产
    UFUNCTION(BlueprintCallable, Category = "GameLogic")
    void StartSpawningWithConfig(UEnemySpawnConfig* Config);

protected:
    void ExecuteSpawn();

private:
    UPROPERTY()
    TArray<TWeakObjectPtr<AActor>> SpawnPoints;

    // 记录当前的配置
    UPROPERTY()
    TObjectPtr<UEnemySpawnConfig> CurrentConfig;

    int32 RemainingCount = 0;
    FTimerHandle SpawnTimerHandle;
};

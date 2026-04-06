// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MyEnemySpawnConfig.generated.h"

/**
 * 
 */
UCLASS()
class CCC_API UMyEnemySpawnConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
    // 刷什么怪
    UPROPERTY(EditAnywhere, Category = "Config")
    TSubclassOf<APawn> EnemyClass;

    // 是否随机选择生成点？
    // true: 从所有点里随机抽一个生
    // false: 在所有已注册的点上同时生
    UPROPERTY(EditAnywhere, Category = "Config")
    bool bIsRandom = true;

    // 这一波总共生成几个怪
    UPROPERTY(EditAnywhere, Category = "Config")
    int32 TotalSpawnCount = 10;

    // 每次生成的间隔时间
    UPROPERTY(EditAnywhere, Category = "Config")
    float Interval = 2.0f;
};

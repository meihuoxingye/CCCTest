// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MyMovementDataAsset.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class CCC_API UMyMovementDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
    /** 常用移动属性 */
    UPROPERTY(EditAnywhere, Category = "Common Movement Properties")
    float MaxWalkSpeed = 600.f;

    UPROPERTY(EditAnywhere, Category = "Common Movement Properties")
    float MaxAcceleration = 2048.f;

    // 停止移动时的减速能力
    UPROPERTY(EditAnywhere, Category = "Common Movement Properties")
    float MoveDeceleration = 2048.f;

    UPROPERTY(EditAnywhere, Category = "Common Movement Properties")
    float JumpSpeed = 700.f;

    // 空中方向控制力
    UPROPERTY(EditAnywhere, Category = "Common Movement Properties")
    float AirControl = 0.2f;

    // 重力缩放
    UPROPERTY(EditAnywhere, Category = "Common Movement Properties")
    float GravityScale = 1.0f;
    /** 常用移动属性 */
	
};

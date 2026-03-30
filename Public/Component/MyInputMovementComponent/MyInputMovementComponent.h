// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MyInputMovementComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class CCC_API UMyInputMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UMyInputMovementComponent();

	// 处理移动与转向
	void HandleMoveInput(const FVector2D& InputAxisVector);




protected:
	// Called when the game starts
	virtual void BeginPlay() override;


    /** 移动与转向函数及参数 */
    // 定时器句柄，用来管理“转身循环”
    FTimerHandle RotationTimerHandle;
    TObjectPtr<APawn> ControlledPawn;

    // 平滑旋转插值函数
    void SmoothRotate();

    // 移动逻辑
    void UpdateMovement(const FVector2D& InputAxisVector);
    // 转向逻辑
    void UpdateRotation(const FVector2D& InputAxisVector);

    // 目标转向角
    float TargetYaw = 0.f;
    // 是否面向右边
    bool bWasFacingRight = true;

    // 平滑转向插值速度
    UPROPERTY(EditAnywhere, Category = "Smooth Rotation Interpolation Parameters")
    float RotationInterpSpeed = 10.f;

    // 更新转向单位计时器时间
    UPROPERTY(EditAnywhere, Category = "Smooth Rotation Interpolation Parameters")
    float Time = 0.01f;
    /** 旋转插值函数及参数 */



private:
    

};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MyMovementControlComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class CCC_API UMyMovementControlComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UMyMovementControlComponent();

    // Called every frame
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// 处理移动与转向
	void HandleMoveInput(const FVector2D& InputAxisVector);

protected:
	// Called when the game starts
	virtual void BeginPlay() override;


    /** 移动与转向函数及参数 */
    // 平滑旋转插值函数
    void SmoothRotate(float Time);

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
    /** 旋转插值函数及参数 */


    /** 缓存找到的指针，避免每帧都寻找 */
    // 缓存 Pawn
    UPROPERTY()
    TObjectPtr<class APawn> CachedControlledPawn;

    // 缓存角色
    UPROPERTY()
    TObjectPtr<class ACharacter> CachedCharacter;

    // 缓存网格
    UPROPERTY()
    TObjectPtr<class USkeletalMeshComponent> CachedMesh;
    /** 缓存找到的自定义输入移动组件，避免每帧都寻找 */

private:
    

};

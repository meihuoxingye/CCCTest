// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BaseCharacter.generated.h"

UCLASS()
class CCC_API ABaseCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ABaseCharacter();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

private:
    /** 常用移动属性 */
    UPROPERTY(EditAnywhere, Category = "Common Movement Properties")
    float MaxWalkSpeed = 600.f;

    UPROPERTY(EditAnywhere, Category = "Common Movement Properties")
    float MaxAcceleration = 2048.f;

    // 停止移动时的减速能力
    UPROPERTY(EditAnywhere, Category = "Common Movement Properties")
    float MoveDeceleration = 2048.f;

    UPROPERTY(EditAnywhere, Category = "Common Movement Properties")
    float JumpSpeed = 420.f;

    // 空中方向控制力
    UPROPERTY(EditAnywhere, Category = "Common Movement Properties")
    float AirControl = 0.2f;

    // 重力缩放
    UPROPERTY(EditAnywhere, Category = "Common Movement Properties")
    float GravityScale = 1.0f;

    // 同步自定义移动属性
    void SyncMovementProperties();
    /** 常用移动属性 */

};

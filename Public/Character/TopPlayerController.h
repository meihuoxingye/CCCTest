// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

// 定义了 FInputActionValue 结构体
// 专门用来装载按键、鼠标位移或手柄摇杆产生的具体数值
#include "InputActionValue.h"

#include "TopPlayerController.generated.h"

/**
 * 
 */
UCLASS()
class CCC_API ATopPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
	ATopPlayerController();

protected:
	virtual void BeginPlay() override;

	TObjectPtr<class ACharacter> MyCharacter;
	TObjectPtr<class UMyInputMovementComponent> UMIMComponent;

	// 键位绑定回调函数
	virtual void SetupInputComponent() override;

private:
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<class UInputMappingContext> TopContext;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<class UInputAction> MoveAction;
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<class UInputAction> JumpAction;
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<class UInputAction> AttackAction;


	/** Input Callback */
	void Move(const FInputActionValue& InputActionValue);
	void Jump();
	void StopJump();
	void Attack();
	void AttackEnd();
};

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

	// 键位绑定回调函数
	virtual void SetupInputComponent() override;


	/** 缓存找到的指针，避免每帧都寻找 */
	// 缓存组件指针，避免每一帧调用 FindComponentByClass
	UPROPERTY()
	TObjectPtr<class UMyMovementControlComponent> CachedMyMovementControlComp;

	// 缓存组件指针
	UPROPERTY()
	TObjectPtr<class UMyCombatComponent> CachedMyCombatComp;

	// 缓存角色指针
	UPROPERTY()
	TObjectPtr<class ATopCharacter> CachedMyCharacter;

	// 当控制器开始控制一个 Pawn 时触发，缓存找到的自定义输入移动组件与角色，只找一次
	virtual void OnPossess(APawn* InPawn) override;
	// 当控制器不再控制时将指针清空
	virtual void OnUnPossess() override;
	/** 缓存找到的自定义输入移动组件，避免每帧都寻找 */

private:
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<class UInputMappingContext> TopContext;
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<class UInputAction> MoveAction;
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<class UInputAction> JumpAction;
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<class UInputAction> AttackAction;


	/** Input Callback Functions*/
	void Move(const FInputActionValue& InputActionValue);
	void Jump();
	void StopJump();
	void Attack();
	void AttackEnd();
};

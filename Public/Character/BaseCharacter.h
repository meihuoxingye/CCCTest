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

	// 常用移动属性组件
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class UMyMovementAttributeComponent> MMAComponent;

	// 自定义战斗组件
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class UMyCombatComponent> MCComponent;

	// 为角色指定专属的初始武器类
	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	TSubclassOf<class AMyWeaponBase> DefaultWeaponClass;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// 给外接开放一个获取武器的接口
	FORCEINLINE TSubclassOf<AMyWeaponBase> GetDefaultWeaponClass() const { return DefaultWeaponClass; }

private:


};

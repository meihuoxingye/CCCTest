// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/BaseCharacter.h"

// 移动组件
#include "GameFramework/CharacterMovementComponent.h"

// 常用移动属性组件
#include "Component/MovementAttribute/MyMovementAttributeComponent.h"

// 自定义战斗组件
#include "Component/CombatSystem/MyCombatComponent.h"

// Sets default values
ABaseCharacter::ABaseCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	MMAComponent = CreateDefaultSubobject<UMyMovementAttributeComponent>(TEXT("常用移动属性组件"));
	MCComponent = CreateDefaultSubobject<UMyCombatComponent>(TEXT("自定义战斗组件"));

}

// Called when the game starts or when spawned
void ABaseCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	// 禁用默认的“面向移动方向”
	GetCharacterMovement()->bOrientRotationToMovement = false;

	// 确保角色不随控制器（鼠标）的旋转而旋转
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	
}

// Called every frame
void ABaseCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void ABaseCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}


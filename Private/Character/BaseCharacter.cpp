// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/BaseCharacter.h"

// 移动组件
#include "GameFramework/CharacterMovementComponent.h"

// Sets default values
ABaseCharacter::ABaseCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

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

	SyncMovementProperties();
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

void ABaseCharacter::SyncMovementProperties()
{
	if (ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
	{
		UCharacterMovementComponent* CMC = OwnerCharacter->GetCharacterMovement();
		if (CMC)
		{
			// 将你的简易参数同步给复杂的原生参数
			CMC->MaxWalkSpeed = MaxWalkSpeed;
			CMC->MaxAcceleration = MaxAcceleration;
			CMC->JumpZVelocity = JumpSpeed;
			CMC->AirControl = AirControl;
			CMC->GravityScale = GravityScale;

			// 别忘了设置刹车，否则停不下来
			CMC->BrakingDecelerationWalking = MoveDeceleration;
		}
	}
}


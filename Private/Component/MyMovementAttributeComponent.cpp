// Fill out your copyright notice in the Description page of Project Settings.


#include "Component/MyMovementAttributeComponent.h"

// 移动组件
#include "GameFramework/CharacterMovementComponent.h"
// 角色
#include "GameFramework/Character.h"

// Sets default values for this component's properties
UMyMovementAttributeComponent::UMyMovementAttributeComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	PrimaryComponentTick.bStartWithTickEnabled = false;
}


// Called when the game starts
void UMyMovementAttributeComponent::BeginPlay()
{
	Super::BeginPlay();

	SyncMovementProperties();
	
}


void UMyMovementAttributeComponent::SyncMovementProperties()
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

// Called every frame
void UMyMovementAttributeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}


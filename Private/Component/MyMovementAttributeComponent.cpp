// Fill out your copyright notice in the Description page of Project Settings.


#include "Component/MyMovementAttributeComponent.h"

// 移动组件
#include "GameFramework/CharacterMovementComponent.h"
// 角色
#include "GameFramework/Character.h"
// 移动数据资产配置
#include "Component/MyInputMovementComponent/MyMovementDataAsset.h"

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
	// 增加安全性检查：确保资产和所有者都存在
	if (MovementConfig && GetOwner())
	{
		if (ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
		{
			UCharacterMovementComponent* CMC = OwnerCharacter->GetCharacterMovement();
			if (CMC)
			{
				// 从数据资产中读取数值并应用
				CMC->MaxWalkSpeed = MovementConfig->MaxWalkSpeed;
				CMC->MaxAcceleration = MovementConfig->MaxAcceleration;
				CMC->JumpZVelocity = MovementConfig->JumpSpeed;
				CMC->AirControl = MovementConfig->AirControl;
				CMC->GravityScale = MovementConfig->GravityScale;
				CMC->BrakingDecelerationWalking = MovementConfig->MoveDeceleration;
			}
		}
	}
	else if (!MovementConfig)
	{
		// 提醒开发者记得在编辑器里指定资产
		UE_LOG(LogTemp, Warning, TEXT("[%s] 未分配 MovementConfig 资产！"), *GetOwner()->GetName());
	}
}

// Called every frame
void UMyMovementAttributeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}


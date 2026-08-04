// Fill out your copyright notice in the Description page of Project Settings.


#include "Component/MovementAttribute/MyMovementAttributeComponent.h"

// 移动组件
#include "GameFramework/CharacterMovementComponent.h"
// 基础角色
#include "Character/BaseCharacter.h"
// 角色属性数据资产配置
#include "Character/CharacterAttributeDataAsset.h"

// Sets default values for this component's properties
UMyMovementAttributeComponent::UMyMovementAttributeComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// ==============================================================================
	// 【废除网络同步】：本组件为“本地数据搬运工”
	// 联机时主客机会在各自的 BeginPlay 中，独立从本地数据资产读取固定数值并填给官方移动组件。
	// 本组件不包含任何 RPC（远程调用）或独有的动态变量，开启网络同步只会白白占用服务器的 Actor Channel。
	// ==============================================================================
	// SetIsReplicatedByDefault(true);
}


// Called when the game starts
void UMyMovementAttributeComponent::BeginPlay()
{
	Super::BeginPlay();

	SyncMovementProperties();
	
}


void UMyMovementAttributeComponent::SyncMovementProperties()
{
	// 增加安全性检查：确保所有者存在
	if (GetOwner())
	{
		if (ABaseCharacter* OwnerCharacter = Cast<ABaseCharacter>(GetOwner()))
		{
			const UCharacterAttributeDataAsset* MovementConfig = OwnerCharacter->GetAttributeConfig();
			UCharacterMovementComponent* CMC = OwnerCharacter->GetCharacterMovement();
			if (CMC && MovementConfig)
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
}

// Called every frame
void UMyMovementAttributeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}


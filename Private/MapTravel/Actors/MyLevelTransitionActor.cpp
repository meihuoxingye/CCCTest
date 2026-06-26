// Fill out your copyright notice in the Description page of Project Settings.

#include "MapTravel/Actors/MyLevelTransitionActor.h"
#include "MapTravel/MyMapTravelSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

// ==============================================================================
// 生命周期与初始化 (Lifecycle & Initialization)
// ==============================================================================
#pragma region

AMyLevelTransitionActor::AMyLevelTransitionActor()
{
	PrimaryActorTick.bCanEverTick = false;

	TargetLevelName = NAME_None;
	TravelPromptText = FText::FromString(TEXT("按 E 前往下一区域"));
}

#pragma endregion


// ==============================================================================
// IMyInteractableInterface 接口实现 (Interactable Interface Implementation)
// ==============================================================================
#pragma region

void AMyLevelTransitionActor::Interact_Implementation(ACharacter* Interactor)
{
	if (TargetLevelName.IsNone())
	{
		return;
	}

	if (Interactor)
	{
		if (UCharacterMovementComponent* MoveComp = Interactor->GetCharacterMovement())
		{
			// 硬性速度检测，拒绝高速冲门造成的物理穿透或相机跳帧
			if (MoveComp->Velocity.Size() > 10.0f)
			{
				return;
			}
			MoveComp->DisableMovement();
		}
	}

	if (UWorld* World = GetWorld())
	{
		if (UMyMapTravelSubsystem* TravelSubsystem = World->GetSubsystem<UMyMapTravelSubsystem>())
		{
			TravelSubsystem->ExecuteMapTravel(TargetLevelName);
		}
	}
}

FText AMyLevelTransitionActor::GetInteractPrompt_Implementation() const
{
	return TravelPromptText;
}

int32 AMyLevelTransitionActor::GetInteractionPriority_Implementation() const
{
	return 100;
}

#pragma endregion
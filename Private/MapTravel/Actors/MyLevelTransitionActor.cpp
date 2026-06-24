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

	// 1. 消除 (Eliminate) 管线启动前，由于按键宏导致的重复交互风险
	if (Interactor)
	{
		if (UCharacterMovementComponent* MoveComp = Interactor->GetCharacterMovement())
		{
			MoveComp->DisableMovement();
		}
	}

	if (UWorld* World = GetWorld())
	{
		if (UMyMapTravelSubsystem* TravelSubsystem = World->GetSubsystem<UMyMapTravelSubsystem>())
		{
			// 发出指令后彻底脱手，无需等待异步回调，直接起飞
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
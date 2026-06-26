// Fill out your copyright notice in the Description page of Project Settings.

#include "MapTravel/Actors/MyZoneTriggerActor.h"
#include "Components/BoxComponent.h"
#include "MapTravel/MyMapTravelSubsystem.h"
#include "WorldPartition/DataLayer/DataLayerAsset.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

// ==============================================================================
// 生命周期与初始化 (Lifecycle & Initialization)
// ==============================================================================
#pragma region

AMyZoneTriggerActor::AMyZoneTriggerActor()
{
	PrimaryActorTick.bCanEverTick = false;

	CollisionComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionComponent"));
	RootComponent = CollisionComponent;

	// 纯碰撞体，无需物理模拟
	CollisionComponent->SetCollisionProfileName(TEXT("Trigger"));
	CollisionComponent->OnComponentBeginOverlap.AddDynamic(this, &AMyZoneTriggerActor::OnOverlapBegin);

	TargetZone = nullptr;
}

#pragma endregion

// ==============================================================================
// 碰撞事件 (Collision Events)
// ==============================================================================
#pragma region

void AMyZoneTriggerActor::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!TargetZone || !OtherActor)
	{
		return;
	}

	APawn* OverlappingPawn = Cast<APawn>(OtherActor);
	if (OverlappingPawn && OverlappingPawn->IsPlayerControlled())
	{
		if (UWorld* World = GetWorld())
		{
			if (UMyMapTravelSubsystem* TravelSubsystem = World->GetSubsystem<UMyMapTravelSubsystem>())
			{
				TravelSubsystem->RefreshSlidingWindow(TargetZone);
			}
		}
	}
}

#pragma endregion
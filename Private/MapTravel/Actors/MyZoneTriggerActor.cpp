// Fill out your copyright notice in the Description page of Project Settings.

#include "MapTravel/Actors/MyZoneTriggerActor.h"
#include "Components/BoxComponent.h"
#include "MapTravel/MyMapTravelSubsystem.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "WorldPartition/DataLayer/DataLayerAsset.h"

// ==============================================================================
// 核心生命周期与组件 (Core Lifecycle & Components)
// ==============================================================================
#pragma region

AMyZoneTriggerActor::AMyZoneTriggerActor()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
}

void AMyZoneTriggerActor::BeginPlay()
{
	Super::BeginPlay();

	TriggerBox->OnComponentBeginOverlap.AddUniqueDynamic(this, &AMyZoneTriggerActor::OnOverlapBegin);
}

#pragma endregion


// ==============================================================================
// 数据层流送逻辑 (Data Layer Streaming Logic)
// ==============================================================================
#pragma region

void AMyZoneTriggerActor::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && OtherActor != this && OtherActor->IsA<ACharacter>())
	{
		// 【注意】：作为滑动窗口流送触发器，玩家可能会来回走动，绝不能像传送门那样关闭碰撞！

		if (UMyMapTravelSubsystem* TravelSubsystem = GetWorld()->GetSubsystem<UMyMapTravelSubsystem>())
		{
			// 【拨乱反正】：将本触发器绑定的数据层，直接喂给子系统的滑动窗口！
			if (AssociatedDataLayer)
			{
				TravelSubsystem->RefreshSlidingWindow(AssociatedDataLayer);
			}
		}
	}
}

#pragma endregion
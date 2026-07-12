// Fill out your copyright notice in the Description page of Project Settings.

#include "MapTravel/Actors/MyZoneTriggerActor.h"
#include "Components/BoxComponent.h"
#include "MapTravel/MyMapTravelSubsystem.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"

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

	// 【修正】：使用 AddUniqueDynamic 
	TriggerBox->OnComponentBeginOverlap.AddUniqueDynamic(this, &AMyZoneTriggerActor::OnOverlapBegin);
}

#pragma endregion


// ==============================================================================
// 转场触发逻辑 (Transition Trigger Logic)
// ==============================================================================
#pragma region

void AMyZoneTriggerActor::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && OtherActor != this && OtherActor->IsA<ACharacter>())
	{
		TriggerBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		if (UMyMapTravelSubsystem* TravelSubsystem = GetWorld()->GetSubsystem<UMyMapTravelSubsystem>())
		{
			// 【终极瘦身】：只传人和坐标，剩下的脏活累活全交给底层和大管家
			TravelSubsystem->ExecuteZoneTravelWithWait(OtherActor, TargetZoneTransform);
		}
	}
}

#pragma endregion


// ==============================================================================
// 转场视觉配置 (Transition Visual Settings)
// ==============================================================================
#pragma region
// （本区无实现代码）
#pragma endregion
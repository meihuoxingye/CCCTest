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
	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AMyZoneTriggerActor::OnOverlapBegin);
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
		// 触碰即锁死防抖
		TriggerBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		if (UMyMapTravelSubsystem* TravelSubsystem = GetWorld()->GetSubsystem<UMyMapTravelSubsystem>())
		{
			// 呼叫大管家执行“同地图带UI瞬移”
			// (这里假设你在 MyMapTravelSubsystem 中实现了 ExecuteZoneTravelWithWait)
			TravelSubsystem->ExecuteZoneTravelWithWait(
				OtherActor,
				TargetZoneTransform,
				ScreenOffUIClass,
				ScreenOffDuration,
				AreaPresentationUIClass,
				MinDisplayTime
			);
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
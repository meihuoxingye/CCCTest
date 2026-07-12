// Fill out your copyright notice in the Description page of Project Settings.

#include "MapTravel/Actors/MyLevelTransitionActor.h"
#include "Components/BoxComponent.h"
#include "MapTravel/MyMapTravelSubsystem.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"

// ==============================================================================
// 核心生命周期与组件 (Core Lifecycle & Components)
// ==============================================================================
#pragma region

AMyLevelTransitionActor::AMyLevelTransitionActor()
{
	// 触发器不需要 Tick，彻底切断节省开销
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;

	// 设置仅产生触发事件，忽略物理碰撞
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
}

void AMyLevelTransitionActor::BeginPlay()
{
	Super::BeginPlay();

	// 【修正】：使用 AddUniqueDynamic 防止 Live Coding 导致的重复绑定崩溃
	TriggerBox->OnComponentBeginOverlap.AddUniqueDynamic(this, &AMyLevelTransitionActor::OnOverlapBegin);
}

#pragma endregion


// ==============================================================================
// 转场触发逻辑 (Transition Trigger Logic)
// ==============================================================================
#pragma region

void AMyLevelTransitionActor::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && OtherActor != this && OtherActor->IsA<ACharacter>())
	{
		TriggerBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		if (UMyMapTravelSubsystem* TravelSubsystem = GetWorld()->GetSubsystem<UMyMapTravelSubsystem>())
		{
			// 【终极瘦身】：只传目标名字，表现层参数大管家会全自动查字典！
			TravelSubsystem->ExecuteMapTravel(TargetLevelName);
		}
	}
}

#pragma endregion


// ==============================================================================
// 转场视觉配置 (Transition Visual Settings)
// ==============================================================================
#pragma region
// （本区仅为 UPROPERTY 属性声明，无具体实现逻辑）
#pragma endregion
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
	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AMyLevelTransitionActor::OnOverlapBegin);
}

#pragma endregion


// ==============================================================================
// 转场触发逻辑 (Transition Trigger Logic)
// ==============================================================================
#pragma region

void AMyLevelTransitionActor::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 仅响应角色 Pawn 的触碰
	if (OtherActor && OtherActor != this && OtherActor->IsA<ACharacter>())
	{
		// 1. 触发后立刻物理锁死碰撞体，防止多名玩家或重复横跳导致二次触发
		TriggerBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		// 2. 呼叫大管家：直接将关卡设计师在面板里配好的参数原封不动地砸过去
		if (UMyMapTravelSubsystem* TravelSubsystem = GetWorld()->GetSubsystem<UMyMapTravelSubsystem>())
		{
			TravelSubsystem->ExecuteMapTravel(
				TargetLevelName,
				ScreenOffUIClass,
				ScreenOffDuration,
				LoadingScreenUIClass,
				MinLoadingTime
			);
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
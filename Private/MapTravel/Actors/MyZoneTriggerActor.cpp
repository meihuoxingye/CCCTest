// Fill out your copyright notice in the Description page of Project Settings.

#include "MapTravel/Actors/MyZoneTriggerActor.h"
#include "Components/BoxComponent.h"
#include "MapTravel/MyMapTravelSubsystem.h"
#include "WorldPartition/DataLayer/DataLayerAsset.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Engine/Engine.h" // 引入全局 GEngine 打印日志所需头文件

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

	// 【核心修复】：强制开启底层重叠事件生成，确保万无一失
	CollisionComponent->SetGenerateOverlapEvents(true);

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
		// 【排错测谎仪】：确保肉身撞线后底层能捕捉到
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("[跃迁雷达触发] 玩家已撞线！正在命令总管预热区域: %s"), *TargetZone->GetName()));
		}

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
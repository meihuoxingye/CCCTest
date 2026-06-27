// Fill out your copyright notice in the Description page of Project Settings.

#include "MapTravel/Actors/MyZoneTriggerActor.h"
#include "Components/BoxComponent.h"
#include "MapTravel/MyMapTravelSubsystem.h"
#include "WorldPartition/DataLayer/DataLayerAsset.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

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

	// 强制开启底层重叠事件生成，确保肉身撞线必定触发
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
	if (!OtherActor) return;

	// 【防线 1】：只要有物理实体切过，先亮黄灯！证明物理碰撞是没问题的！
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Yellow, FString::Printf(TEXT("[物理雷达] 检测到 %s 撞击了雷达边缘！"), *OtherActor->GetName()));

	// 【防线 2】：拔除静默杀手！如果没填资产，直接大红字报警！
	if (!TargetZone)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("[严重致命] 雷达罢工：TargetZone 为空！你绝对是忘了在编辑器细节面板里给这个雷达填入绿色资产了！"));
		return;
	}

	APawn* OverlappingPawn = Cast<APawn>(OtherActor);
	if (OverlappingPawn && OverlappingPawn->IsPlayerControlled())
	{
		// 【防线 3】：确认是主角，向管家发信号
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("[跃迁雷达触发] 玩家已撞线！正在命令总管预热区域: %s"), *TargetZone->GetName()));

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
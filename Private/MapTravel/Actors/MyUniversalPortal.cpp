// Fill out your copyright notice in the Description page of Project Settings.

#include "MapTravel/Actors/MyUniversalPortal.h"
#include "MapTravel/MyMapTravelSubsystem.h"
#include "MapTravel/DataAsset/TeleportRoute.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

// ==============================================================================
// 核心生命周期与组件 (Core Lifecycle & Components)
// ==============================================================================
#pragma region

AMyUniversalPortal::AMyUniversalPortal()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
}

void AMyUniversalPortal::BeginPlay()
{
	Super::BeginPlay();

	TriggerBox->OnComponentBeginOverlap.AddUniqueDynamic(this, &AMyUniversalPortal::OnOverlapBegin);
}

#pragma endregion


// ==============================================================================
// 传送发射配置 (Teleport Emitting Configuration)
// ==============================================================================
#pragma region

void AMyUniversalPortal::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	APawn* OverlappedPawn = Cast<APawn>(OtherActor);

	// 拦截过滤：仅允许本地受控玩家触发，防死 AI 和物理碎块
	if (OverlappedPawn && OverlappedPawn->IsLocallyControlled())
	{
		// 校验防呆：确保已配置合法路由
		if (EmittingRoute)
		{
			// 物理封锁：一旦触发立即关闭碰撞，绝对防止玩家在黑屏期间抖动导致二次重入
			TriggerBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);

			if (UMyMapTravelSubsystem* TravelSubsystem = GetWorld()->GetSubsystem<UMyMapTravelSubsystem>())
			{
				// 呼叫大管家：抛射路由指针，由底层系统全权接管寻址
				TravelSubsystem->ExecuteUniversalTravel(OtherActor, EmittingRoute);
			}
		}
	}
}

#pragma endregion
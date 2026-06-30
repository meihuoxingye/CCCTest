// Fill out your copyright notice in the Description page of Project Settings.

#include "MapTravel/Actors/MyLevelTransitionActor.h"
#include "MapTravel/MyMapTravelSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Blueprint/UserWidget.h"

// ==============================================================================
// 生命周期与初始化 (Lifecycle & Initialization)
// ==============================================================================
#pragma region

AMyLevelTransitionActor::AMyLevelTransitionActor()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;

	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
	TriggerBox->SetGenerateOverlapEvents(true);
	TriggerBox->SetBoxExtent(FVector(100.f, 100.f, 100.f));

	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AMyLevelTransitionActor::OnOverlapBegin);

	TargetLevelName = NAME_None;
	bHasTriggered = false;
}

void AMyLevelTransitionActor::BeginPlay()
{
	Super::BeginPlay();

	// 【预热防线】：在地图刚加载时，提前将 UI 读入内存，彻底消除撞线瞬间的 IO 卡顿！
	if (!TransitionSpecificUI.IsNull())
	{
		TransitionSpecificUI.LoadSynchronous();
	}
}

#pragma endregion

// ==============================================================================
// 碰撞触发逻辑 (Collision Events)
// ==============================================================================
#pragma region

void AMyLevelTransitionActor::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 【物理门控防御】：一生只触发一次，且屏蔽无效实体
	if (bHasTriggered || !OtherActor) return;

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Yellow, FString::Printf(TEXT("[物理雷达] 捕捉到物体切入: %s"), OtherActor ? *OtherActor->GetName() : TEXT("未知")));
	}

	if (TargetLevelName.IsNone())
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("[严重错误] TargetLevelName 为空！跃迁阻断，请在细节面板填写目标地图名！"));
		}
		return;
	}

	if (ACharacter* PlayerCharacter = Cast<ACharacter>(OtherActor))
	{
		// 【多玩家网络防御】：严格限制只有“本地控制”的玩家肉身撞线才能拉起系统！
		if (PlayerCharacter->IsPlayerControlled() && PlayerCharacter->IsLocallyControlled())
		{
			bHasTriggered = true; // 触发锁死，钉死状态！

			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("[跃迁启动] 正在剥夺玩家控制权，准备传送至: %s"), *TargetLevelName.ToString()));
			}

			if (UCharacterMovementComponent* MoveComp = PlayerCharacter->GetCharacterMovement())
			{
				MoveComp->DisableMovement();
				MoveComp->StopMovementImmediately(); // 表现层隔离，防止冲出边界
			}

			if (UWorld* World = GetWorld())
			{
				if (UMyMapTravelSubsystem* TravelSubsystem = World->GetSubsystem<UMyMapTravelSubsystem>())
				{
					// 【核心传递】：将地图名、专属UI、以及设定的等待时间，全部喂给大管家！
					TravelSubsystem->ExecuteMapTravel(TargetLevelName, TransitionSpecificUI, MinimumLoadingTime);
				}
			}
		}
	}
}

#pragma endregion
// Fill out your copyright notice in the Description page of Project Settings.

#include "MapTravel/Actors/MyLevelTransitionActor.h"
#include "Components/BoxComponent.h"
#include "MapTravel/MyMapTravelSubsystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

// ==============================================================================
// 核心生命周期与组件 (Core Lifecycle & Components)
// ==============================================================================
#pragma region

AMyLevelTransitionActor::AMyLevelTransitionActor()
{
	// 触发器不需要 Tick，彻底切断节省开销
	PrimaryActorTick.bCanEverTick = false;

	// 实例化底层的 Box 碰撞组件
	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));

	// 将 Box 碰撞组件设为当前 Actor 的物理和空间根组件
	RootComponent = TriggerBox;

	// 设置仅产生触发事件，忽略物理碰撞，防止阻挡玩家或 AI 正常移动
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
}

void AMyLevelTransitionActor::BeginPlay()
{
	Super::BeginPlay();

	// 使用 AddUniqueDynamic 防止 Live Coding 或热重载时触发重复绑定导致的断言崩溃
	TriggerBox->OnComponentBeginOverlap.AddUniqueDynamic(this, &AMyLevelTransitionActor::OnOverlapBegin);
}

#pragma endregion


// ==============================================================================
// 转场触发逻辑 (Transition Trigger Logic)
// ==============================================================================
#pragma region

void AMyLevelTransitionActor::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 将重叠到的 Actor 尝试强转为 Pawn
	APawn* OverlappedPawn = Cast<APawn>(OtherActor);

	// 判定条件的精确化：必须确保重叠的 Actor 是受本地玩家控制的 Pawn，防止 AI 巡逻员意外触发转场
	if (OverlappedPawn && OverlappedPawn->IsLocallyControlled())
	{
		// 状态锁：一旦触发，立刻物理关闭此触发器的碰撞检测，防止玩家在转场前疯狂重叠导致撞车
		TriggerBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		// 安全获取当前世界的转场子系统大管家
		if (UMyMapTravelSubsystem* TravelSubsystem = GetWorld()->GetSubsystem<UMyMapTravelSubsystem>())
		{
			// 终极瘦身：只传目标名字，表现层参数大管家会全自动查字典
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
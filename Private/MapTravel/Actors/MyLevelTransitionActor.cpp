// Fill out your copyright notice in the Description page of Project Settings.

#include "MapTravel/Actors/MyLevelTransitionActor.h"
#include "MapTravel/MyMapTravelSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/BoxComponent.h"
// 【新增：屏幕文本输出所需的全局引擎头文件】
#include "Engine/Engine.h"
#include "Blueprint/UserWidget.h" // 【新增】：引入 UUserWidget

// ==============================================================================
// 生命周期与初始化 (Lifecycle & Initialization)
// ==============================================================================
#pragma region

AMyLevelTransitionActor::AMyLevelTransitionActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// 1. 创建物理碰撞盒作为根节点
	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;

	// 2. 配置物理属性为触发器，强制允许重叠事件
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
	TriggerBox->SetGenerateOverlapEvents(true);
	TriggerBox->SetBoxExtent(FVector(100.f, 100.f, 100.f));

	// 3. 绑定 C++ 重叠事件
	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AMyLevelTransitionActor::OnOverlapBegin);

	TargetLevelName = NAME_None;
}

#pragma endregion

// ==============================================================================
// 碰撞触发逻辑 (Collision Events)
// ==============================================================================
#pragma region

void AMyLevelTransitionActor::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 【测谎仪 1】：只要有物理物体切过边缘，立刻打印黄色警告
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Yellow, FString::Printf(TEXT("[物理雷达] 捕捉到物体切入: %s"), OtherActor ? *OtherActor->GetName() : TEXT("未知")));
	}

	if (!OtherActor) return;

	// 【测谎仪 2】：检查是不是忘记填地图名字了
	if (TargetLevelName.IsNone())
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("[严重错误] TargetLevelName 为空！跃迁阻断，请在细节面板填写目标地图名！"));
		}
		return;
	}

	// 确认是玩家本人
	if (ACharacter* PlayerCharacter = Cast<ACharacter>(OtherActor))
	{
		if (PlayerCharacter->IsPlayerControlled())
		{
			// 【测谎仪 3】：逻辑全通，开始切图
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("[跃迁启动] 正在剥夺玩家控制权，准备传送至: %s"), *TargetLevelName.ToString()));
			}

			if (UCharacterMovementComponent* MoveComp = PlayerCharacter->GetCharacterMovement())
			{
				MoveComp->DisableMovement();
			}

			if (UWorld* World = GetWorld())
			{
				if (UMyMapTravelSubsystem* TravelSubsystem = World->GetSubsystem<UMyMapTravelSubsystem>())
				{
					// 【核心修正】：将该传送门配置的专属 UI 喂给大管家！
					TravelSubsystem->ExecuteMapTravel(TargetLevelName, TransitionSpecificUI);
				}
			}
		}
	}
}

#pragma endregion
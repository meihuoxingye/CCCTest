// Fill out your copyright notice in the Description page of Project Settings.

#include "MapTravel/Actors/MyZoneTriggerActor.h"
#include "Components/BoxComponent.h"
#include "MapTravel/MyMapTravelSubsystem.h"
#include "WorldPartition/DataLayer/DataLayerAsset.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "TimerManager.h" // 必须包含以支持定时器
#include "Blueprint/UserWidget.h" // 必须包含以支持 UI 软指针

// ==============================================================================
// 生命周期与初始化 (Lifecycle & Initialization)
// ==============================================================================
#pragma region

AMyZoneTriggerActor::AMyZoneTriggerActor()
{
	PrimaryActorTick.bCanEverTick = false;

	CollisionComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionComponent"));
	RootComponent = CollisionComponent;

	CollisionComponent->SetCollisionProfileName(TEXT("Trigger"));
	CollisionComponent->SetGenerateOverlapEvents(true);

	CollisionComponent->OnComponentBeginOverlap.AddDynamic(this, &AMyZoneTriggerActor::OnOverlapBegin);

	TargetZone = nullptr;
	WaitTime = 0.0f;
	TriggerCooldown = 1.0f;
	bIsOnCooldown = false;
	bHasTriggeredHardTravel = false;
}

void AMyZoneTriggerActor::BeginPlay()
{
	Super::BeginPlay();

	// 【预热防线】：只有在开启了硬切换 (WaitTime > 0) 时，才提前加载 UI 进内存
	if (WaitTime > 0.0f && !TransitionSpecificUI.IsNull())
	{
		TransitionSpecificUI.LoadSynchronous();
	}
}

#pragma endregion

// ==============================================================================
// 内部防抖状态锁 (Internal Debounce State)
// ==============================================================================
#pragma region

void AMyZoneTriggerActor::ResetTriggerCooldown()
{
	bIsOnCooldown = false;
}

#pragma endregion

// ==============================================================================
// 碰撞事件 (Collision Events)
// ==============================================================================
#pragma region

void AMyZoneTriggerActor::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 【物理门控】：过滤无效实体、冷却期、以及已触发的硬切换
	if (bHasTriggeredHardTravel || bIsOnCooldown || !OtherActor) return;

	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Yellow, FString::Printf(TEXT("[物理雷达] 检测到 %s 撞击了雷达边缘！"), *OtherActor->GetName()));

	if (!TargetZone)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("[严重致命] 雷达罢工：TargetZone 为空！你绝对是忘了在编辑器细节面板里给这个雷达填入绿色资产了！"));
		return;
	}

	APawn* OverlappingPawn = Cast<APawn>(OtherActor);
	// 【本地网络防御】：只有当前客户端控制的角色撞线才算数！
	if (OverlappingPawn && OverlappingPawn->IsPlayerControlled() && OverlappingPawn->IsLocallyControlled())
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("[跃迁雷达触发] 玩家已撞线！正在命令总管预热区域: %s"), *TargetZone->GetName()));

		if (UWorld* World = GetWorld())
		{
			if (UMyMapTravelSubsystem* TravelSubsystem = World->GetSubsystem<UMyMapTravelSubsystem>())
			{
				// 【完美区分你的两种情况】
				if (WaitTime > 0.0f)
				{
					// 需求 2：同一地图不同关卡 -> 需要拉黑屏、锁按键、强制等待！
					bHasTriggeredHardTravel = true; // 钉死状态，一生只进一次
					TravelSubsystem->ExecuteZoneTravelWithWait(TargetZone, TransitionSpecificUI, WaitTime);
				}
				else
				{
					// 需求 1：同一关卡不同区域 -> 瞬间静默加载
					bIsOnCooldown = true; // 启动防抖冷却，屏蔽后续几百毫秒内的物理摩擦重入
					World->GetTimerManager().SetTimer(CooldownTimerHandle, this, &AMyZoneTriggerActor::ResetTriggerCooldown, TriggerCooldown, false);
					TravelSubsystem->RefreshSlidingWindow(TargetZone);
				}
			}
		}
	}
}

#pragma endregion
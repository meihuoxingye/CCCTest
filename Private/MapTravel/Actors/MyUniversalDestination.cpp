// Fill out your copyright notice in the Description page of Project Settings.

#include "MapTravel/Actors/MyUniversalDestination.h"
#include "MapTravel/MyMapTravelSubsystem.h"
#include "MapTravel/DataAsset/TeleportRoute.h"
#include "Game/MyGameInstance.h"
#include "Engine/World.h"

// ==============================================================================
// 核心生命周期与组件 (Core Lifecycle & Components)
// ==============================================================================
#pragma region

AMyUniversalDestination::AMyUniversalDestination()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AMyUniversalDestination::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	if (!World) return;

	// 本地注册管线：遍历自身监听的所有路由，向当前世界的子系统进行高速指针映射注册
	if (UMyMapTravelSubsystem* TravelSubsystem = World->GetSubsystem<UMyMapTravelSubsystem>())
	{
		for (UTeleportRoute* Route : ListeningRoutes)
		{
			if (Route)
			{
				TravelSubsystem->RegisterSameMapDestination(Route, this, GetActorTransform());
			}
		}
	}

	// 跨地图落地接机管线：在世界生成的第一帧，搜查全局大管家手中的跨界车票
	if (UMyGameInstance* GI = World->GetGameInstance<UMyGameInstance>())
	{
		// 校验大管家手中持有的跨界车票是否属于自己的监听阵列
		if (GI->PendingTravelRoute != nullptr && ListeningRoutes.Contains(GI->PendingTravelRoute))
		{
			// 撕毁车票：清除全局跨界状态，防止复活或二次加载时引发幽灵瞬移
			GI->PendingTravelRoute = nullptr;

			// 零感知瞬移：在玩家睁眼前，调用最高物理权限将其强行吸附至此锚点
			if (APlayerController* PC = World->GetFirstPlayerController())
			{
				if (APawn* Pawn = PC->GetPawn())
				{
					Pawn->SetActorTransform(GetActorTransform());
				}
			}
		}
	}
}

void AMyUniversalDestination::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 物理注销：自身被销毁时，立即清洗本地注册表，绝对杜绝野指针崩溃
	if (UWorld* World = GetWorld())
	{
		if (UMyMapTravelSubsystem* TravelSubsystem = World->GetSubsystem<UMyMapTravelSubsystem>())
		{
			for (UTeleportRoute* Route : ListeningRoutes)
			{
				if (Route)
				{
					TravelSubsystem->UnregisterSameMapDestination(Route);
				}
			}
		}
	}

	Super::EndPlay(EndPlayReason);
}

#pragma endregion
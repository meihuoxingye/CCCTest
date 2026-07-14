// Fill out your copyright notice in the Description page of Project Settings.

#include "MapTravel/Actors/MyUniversalDestination.h"
#include "MapTravel/MyMapTravelSubsystem.h"
#include "MapTravel/DataAsset/TeleportRoute.h"
#include "Game/MyGameInstance.h"
#include "Engine/World.h"

// ==========================================
// 补充以下两个头文件，消除不完整类型报错
#include "GameFramework/Pawn.h"               
//#include "GameFramework/PlayerController.h"   
// ==========================================

// ==============================================================================
// 核心生命周期与组件 (Core Lifecycle & Components)
// ==============================================================================
#pragma region

AMyUniversalDestination::AMyUniversalDestination()
{
	PrimaryActorTick.bCanEverTick = false;

	// 【新增核心修复】：禁止被大世界分区卸载，保证跨图生成的第一帧它绝对在场！
	bIsSpatiallyLoaded = false;
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

	// 注意：跨地图落地接机的烂代码已全部物理切除！寻址现已全盘移交 GameMode！
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
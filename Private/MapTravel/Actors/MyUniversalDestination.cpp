// Fill out your copyright notice in the Description page of Project Settings.

#include "MapTravel/Actors/MyUniversalDestination.h"
#include "MapTravel/MyMapTravelSubsystem.h"
#include "MapTravel/DataAsset/TeleportRoute.h"
#include "Engine/World.h"

// 补充以下两个头文件，消除不完整类型报错      
#include "WorldPartition/DataLayer/DataLayerAsset.h" // 新增引入


// ==============================================================================
// 核心生命周期与组件 (Core Lifecycle & Components)
// ==============================================================================
#pragma region

AMyUniversalDestination::AMyUniversalDestination()
{
	PrimaryActorTick.bCanEverTick = false;

	// 【必须加回这个物理根节点！】
	// 无论蓝图怎么配，C++底层必须拥有绝对的空间锚点，否则大管家抓取坐标必定报错！
	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	// 【新增核心修复】：禁止被大世界分区卸载，保证跨图生成的第一帧它绝对在场！
	bIsSpatiallyLoaded = false;
}

void AMyUniversalDestination::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	if (!World) return;


	UE_LOG(LogTemp, Warning, TEXT("[MapTravelLog] 📍 目标点 [%s] 成功在世界中 BeginPlay 醒来！坐标: %s"), *GetName(), *GetActorLocation().ToString());

	// 本地注册管线：遍历自身监听的所有路由，向当前世界的子系统进行高速指针映射注册
	if (UMyMapTravelSubsystem* TravelSubsystem = World->GetSubsystem<UMyMapTravelSubsystem>())
	{
		for (UTeleportRoute* Route : ListeningRoutes)
		{
			if (Route)
			{
				/**/
				UE_LOG(LogTemp, Warning, TEXT("[MapTravelLog] -> 目标点 [%s] 成功向字典注册路由: [%s]"), *GetName(), *Route->GetName());

				// 【修改】：将绑定的数据层一同传递进注册中心
				TravelSubsystem->RegisterSameMapDestination(Route, this, GetActorTransform(), BoundDataLayer);
			}
			else
			{
				/**/
				UE_LOG(LogTemp, Error, TEXT("[MapTravelLog] -> 错误！目标点 [%s] 的 ListeningRoutes 数组中存在空指针 (未配置路由资产)！"), *GetName());
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
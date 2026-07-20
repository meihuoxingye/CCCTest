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
	// 性能极简：作为纯粹的物理空间锚点和数据容器，绝对不需要每帧更新，强行关闭 Tick 释放 CPU 算力
	PrimaryActorTick.bCanEverTick = false;

	// 【必须加回这个物理根节点！】
	// 无论蓝图怎么配，C++底层必须拥有绝对的空间锚点，否则大管家抓取坐标必定报错！
	// 实例化原生的场景组件，为其在虚幻的三维世界中提供最基础的 Transform（坐标、旋转、缩放）数据
	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	// 【新增核心修复】：禁止被大世界分区卸载，保证跨图生成的第一帧它绝对在场！
	// 突破 World Partition 的网格流送限制，强制将其标记为常驻内存，防止跨图落地时因该地块尚未加载而抓不到接机点
	bIsSpatiallyLoaded = false;
}

void AMyUniversalDestination::BeginPlay()
{
	// 调用父类的原生 BeginPlay，确保底层 Actor 状态机正常拉起
	Super::BeginPlay();

	// 安全获取当前世界的上下文，防范处于编辑器预览等异常状态
	UWorld* World = GetWorld();
	// 如果世界无效，直接切断后续注册逻辑
	if (!World) return;


	UE_LOG(LogTemp, Warning, TEXT("[MapTravelLog] 📍 目标点 [%s] 成功在世界中 BeginPlay 醒来！坐标: %s"), *GetName(), *GetActorLocation().ToString());

	// 本地注册管线：遍历自身监听的所有路由，向当前世界的子系统进行高速指针映射注册
	// 跨系统通信：精准抓取负责大世界底层数据层流送的大一统传送子系统
	if (UMyMapTravelSubsystem* TravelSubsystem = World->GetSubsystem<UMyMapTravelSubsystem>())
	{
		// 遍历该接机点在细节面板中配置的所有“监听路由”数组
		for (UTeleportRoute* Route : ListeningRoutes)
		{
			// 校验防呆：确保策划配置的数组元素不是空头支票（Nullptr）
			if (Route)
			{
				UE_LOG(LogTemp, Warning, TEXT("[MapTravelLog] -> 目标点 [%s] 成功向字典注册路由: [%s]"), *GetName(), *Route->GetName());

				// 【修改】：将绑定的数据层一同传递进注册中心
				// 核心入库操作：将当前的路由资产作为 Key，将自身的物理指针、绝对三维坐标、以及绑定的流送数据层，一股脑塞入子系统的高速哈希表中
				TravelSubsystem->RegisterSameMapDestination(Route, this, GetActorTransform(), BoundDataLayer);
			}
			else
			{
				// 如果策划在数组里留了空元素，抛出显眼的红色错误日志强行警告
				UE_LOG(LogTemp, Error, TEXT("[MapTravelLog] -> 错误！目标点 [%s] 的 ListeningRoutes 数组中存在空指针 (未配置路由资产)！"), *GetName());
			}
		}
	}

	// 注意：跨地图落地接机的烂代码已全部物理切除！寻址现已全盘移交 GameMode！
}

void AMyUniversalDestination::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 物理注销：自身被销毁时，立即清洗本地注册表，绝对杜绝野指针崩溃
	// 哪怕世界正在被销毁 (EndPlayReason::Destroyed / LevelUnloaded)，也必须尝试安全获取世界上下文
	if (UWorld* World = GetWorld())
	{
		// 抓取传送子系统
		if (UMyMapTravelSubsystem* TravelSubsystem = World->GetSubsystem<UMyMapTravelSubsystem>())
		{
			// 遍历自身曾经注册过的所有路由资产
			for (UTeleportRoute* Route : ListeningRoutes)
			{
				// 再次校验路由指针的有效性
				if (Route)
				{
					// 从子系统的高速字典中物理擦除这组 Key-Value 映射，彻底切断强引用，防止产生幽灵接机点
					TravelSubsystem->UnregisterSameMapDestination(Route);
				}
			}
		}
	}

	// 将剩余的销毁收尾工作交还给引擎底层的原生 EndPlay
	Super::EndPlay(EndPlayReason);
}

#pragma endregion
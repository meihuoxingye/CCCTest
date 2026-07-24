// Fill out your copyright notice in the Description page of Project Settings.

#include "Streaming/MyZoneTriggerActor.h"
#include "Components/BoxComponent.h"
// 【修改】：原本包含的是 MyMapTravelSubsystem，现在改为流送专用子系统
#include "Streaming/MyDataLayerStreamingSubsystem.h" 
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "WorldPartition/DataLayer/DataLayerAsset.h"

// ==============================================================================
// 核心生命周期与组件 (Core Lifecycle & Components)
// ==============================================================================
#pragma region

AMyZoneTriggerActor::AMyZoneTriggerActor()
{
	// 触发器本身不需要执行任何逐帧逻辑，彻底切断 Tick 以极致节省 CPU 开销
	PrimaryActorTick.bCanEverTick = false;

	// 实例化底层的盒体碰撞组件
	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));

	// 将盒体碰撞组件设为当前 Actor 的物理和空间层级根节点
	RootComponent = TriggerBox;

	// 赋予触发器专属碰撞预设，仅产生重叠事件，绝对忽略物理阻挡
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
}

void AMyZoneTriggerActor::BeginPlay()
{
	Super::BeginPlay();

	// 使用 AddUniqueDynamic 进行绑定，防止 Live Coding 或编辑器热重载时产生重复绑定导致的多次触发与内存崩溃
	TriggerBox->OnComponentBeginOverlap.AddUniqueDynamic(this, &AMyZoneTriggerActor::OnOverlapBegin);
}

#pragma endregion


// ==============================================================================
// 数据层流送逻辑 (Data Layer Streaming Logic)
// ==============================================================================
#pragma region

void AMyZoneTriggerActor::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 尝试将重叠进来的 Actor 转换为基础肉体 (Pawn)
	APawn* OverlappedPawn = Cast<APawn>(OtherActor);

	// 判定条件的精确化：必须确保重叠者是合法且受本地玩家控制的 Pawn
	// 这一步彻底防死了 AI 巡逻员、掉落的武器或乱跑的 NPC 意外拉动世界数据层的流送
	if (OverlappedPawn && OverlappedPawn->IsLocallyControlled())
	{
		// 注意：作为滑动窗口流送触发器，玩家在探索时必然会来回走动并反复进出该区域
		// 绝不能像地图传送门那样在触发后关闭碰撞 (SetCollisionEnabled)，必须保持长效监听！

		// 【修改】：安全获取全新的数据层流送大管家
		if (UMyDataLayerStreamingSubsystem* StreamingSubsystem = GetWorld()->GetSubsystem<UMyDataLayerStreamingSubsystem>())
		{
			// 确保关卡设计师在细节面板里正确绑定了目标数据层资产
			if (AssociatedDataLayer)
			{
				// 拨乱反正：将本触发器绑定的核心数据层喂给流送大管家的滑动窗口
				// 由流送子系统统一推演和接管远近格子的内存分配，实现极低耦合
				StreamingSubsystem->RefreshSlidingWindow(AssociatedDataLayer);
			}
		}
	}
}

#pragma endregion
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
	// 性能极简：关闭 Actor 的每帧 Tick 计算，传送门作为纯静态触发器绝对不需要帧更新，极致节省 CPU 算力
	PrimaryActorTick.bCanEverTick = false;

	// 实例化一个盒型碰撞体组件，作为传送门的核心物理检测边界
	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));

	// 将触发盒子提升为该 Actor 的物理根节点，使其主导整个传送门在世界中的绝对坐标与旋转
	RootComponent = TriggerBox;

	// 赋予其虚幻预设的 "Trigger" 碰撞配置文件，专职负责重叠 (Overlap) 事件，绝对不阻挡玩家正常的物理移动
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
}

void AMyUniversalPortal::BeginPlay()
{
	// 调用父类的原生 BeginPlay，确保引擎底层完成基础初始化
	Super::BeginPlay();

	// 物理钩子注入：动态绑定重叠事件委托
	// 当任何物理形体跨入 TriggerBox 边界时，立刻回调本类的 OnOverlapBegin 函数
	// 使用 AddUniqueDynamic 保证唯一性，杜绝多次初始化导致的事件重入与重复触发
	TriggerBox->OnComponentBeginOverlap.AddUniqueDynamic(this, &AMyUniversalPortal::OnOverlapBegin);
}

#pragma endregion


// ==============================================================================
// 传送发射配置 (Teleport Emitting Configuration)
// ==============================================================================
#pragma region

void AMyUniversalPortal::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 尝试将触发重叠的未知 Actor 安全转换为 Pawn（可受控肉体）基类，以此筛除掉无生命的场景道具或飞行子弹
	APawn* OverlappedPawn = Cast<APawn>(OtherActor);

	// 拦截过滤：仅允许本地受控玩家触发，防死 AI 和物理碎块
	if (OverlappedPawn && OverlappedPawn->IsLocallyControlled())
	{
		// 校验防呆：确保已配置合法路由
		if (EmittingRoute)
		{
			// 【删掉那句 TriggerBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);】
			// 绝对不要在这里永久关闭碰撞！防连踩的互斥锁（bIsTraveling）已经在 TravelSubsystem 里做得很完美了。

			// 安全获取当前世界的上下文，并提取负责统筹全局流送的大一统传送子系统
			if (UMyMapTravelSubsystem* TravelSubsystem = GetWorld()->GetSubsystem<UMyMapTravelSubsystem>())
			{
				// 呼叫大管家：抛射路由指针，由底层系统全权接管寻址
				TravelSubsystem->ExecuteUniversalTravel(OtherActor, EmittingRoute);
			}
		}
	}
}

#pragma endregion
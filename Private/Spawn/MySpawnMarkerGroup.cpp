// Fill out your copyright notice in the Description page of Project Settings.


#include "Spawn/MySpawnMarkerGroup.h"
#include "Components/ArrowComponent.h"     // 引入箭头组件
#include "Components/BillboardComponent.h" // 引入图标组件
#include "Spawn/MyEnemySpawnSubsystem.h"

AMySpawnMarkerGroup::AMySpawnMarkerGroup()
{
    // 关闭 Tick，追求极致性能
    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;

    // 创建根节点
    RootComp = CreateDefaultSubobject<USceneComponent>(TEXT("RootComp"));
    RootComponent = RootComp;

    // 编译器开关
    // 在编辑器里会运行，打包出来的游戏不会运行
#if WITH_EDITORONLY_DATA
    // 告示板组件，在场景里看到的“小灯泡”、“小喇叭”、“小旗子”，它们都是 UBillboardComponent
    ManagerIcon = CreateDefaultSubobject<UBillboardComponent>(TEXT("ManagerIcon"));
    ManagerIcon->SetupAttachment(RootComponent);
#endif
}

void AMySpawnMarkerGroup::BeginPlay()
{
    Super::BeginPlay();

    // 游戏开始时，获取刷怪子系统
    if (UMyEnemySpawnSubsystem* Sub = GetWorld()->GetSubsystem<UMyEnemySpawnSubsystem>())
    {
        // 获取自己身上挂载的所有“箭头组件” (ArrowComponent)
        TArray<UArrowComponent*> ArrowComps;
        GetComponents<UArrowComponent>(ArrowComps);

        // 遍历这些箭头，把它们作为空间坐标上报给子系统并注册
        for (UArrowComponent* Arrow : ArrowComps)
        {
            Sub->RegisterSpawnPoint(Arrow);
        }
    }
}

void AMySpawnMarkerGroup::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 游戏结束或此管理器被销毁时，将自己身上的箭头从子系统中注销
    if (UMyEnemySpawnSubsystem* Sub = GetWorld()->GetSubsystem<UMyEnemySpawnSubsystem>())
    {
        TArray<UArrowComponent*> ArrowComps;
        GetComponents<UArrowComponent>(ArrowComps);

        for (UArrowComponent* Arrow : ArrowComps)
        {
            Sub->UnregisterSpawnPoint(Arrow);
        }
    }

    Super::EndPlay(EndPlayReason);
}
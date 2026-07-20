// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MyUniversalPortal.generated.h"

class UBoxComponent;
class UTeleportRoute;

/**
 * 大一统传送门 (Universal Portal)
 * 物理世界的入口锚点，负责监听玩家踩踏事件，并向大管家提交路由寻址请求。
 */
UCLASS()
class CCC_API AMyUniversalPortal : public AActor
{
	GENERATED_BODY()

	// ==============================================================================
	// 核心生命周期与组件 (Core Lifecycle & Components)
	// ==============================================================================
public:

	// 构造函数：初始化传送门的基础物理组件与默认属性
	AMyUniversalPortal();

protected:

	// 原生生命周期覆写：游戏启动、实体生成时调用，用于绑定触发器的物理重叠监听委托
	virtual void BeginPlay() override;

	// 核心物理触发组件：作为传送门的绝对空间范围，玩家肉体碰触此框即视为踩入传送阵
	// 使用 UBoxComponent 提供精准且低开销的 AABB/OBB 碰撞计算
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* TriggerBox;


	// ==============================================================================
	// 传送发射配置 (Teleport Emitting Configuration)
	// ==============================================================================
public:

	// 发射路由：策划只需拖入目标路由资产，该传送门自动绑定物理路线
	// （解耦核心：传送门本身既不知道目标坐标，也不知道是同图还是跨图，只负责“发车票”）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Teleport")
	UTeleportRoute* EmittingRoute;

protected:

	// 物理碰撞回调函数：当有物理实体（如玩家）切入 TriggerBox 内部时，由引擎底层物理系统瞬间回调
	// 内部逻辑负责拦截并过滤非玩家实体（如 AI、子弹、碎块），随后将寻址任务全权抛给子系统
	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
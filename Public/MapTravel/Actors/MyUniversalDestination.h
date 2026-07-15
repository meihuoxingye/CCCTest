// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MyUniversalDestination.generated.h"


class UTeleportRoute;
class UDataLayerAsset; // 新增：数据层前向声明


UCLASS()
class CCC_API AMyUniversalDestination : public AActor
{
	GENERATED_BODY()

	// ==============================================================================
	// 核心生命周期与组件 (Core Lifecycle & Components)
	// ==============================================================================
public:

	AMyUniversalDestination();

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;


	// ==============================================================================
	// 传送监听配置 (Teleport Listening Configuration)
	// ==============================================================================
public:

	// 路由监听矩阵：允许策划拖入多个路由资产，实现多路汇聚的高级拓扑结构
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Teleport")
	TArray<UTeleportRoute*> ListeningRoutes;

	// 【新增】目标数据层绑定：传送至此时，将自动触发该层的激活，并卸载远端数据层
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Teleport")
	UDataLayerAsset* BoundDataLayer = nullptr;
};
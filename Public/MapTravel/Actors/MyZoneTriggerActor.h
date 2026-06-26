// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MyZoneTriggerActor.generated.h"

class UBoxComponent;
class UDataLayerAsset;

/**
 * 2.5D 全自动空间感知触发器 (Zone Proximity Trigger)
 * 放置在路段中点，静默刷新滑动窗口，彻底消除手动维护脚本的负担
 */
UCLASS()
class CCC_API AMyZoneTriggerActor : public AActor
{
	GENERATED_BODY()

	// ==============================================================================
	// 生命周期与初始化 (Lifecycle & Initialization)
	// ==============================================================================
public:
	AMyZoneTriggerActor();

	// ==============================================================================
	// 组件与属性 (Components & Properties)
	// ==============================================================================
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trigger")
	TObjectPtr<UBoxComponent> CollisionComponent;

	// 触发器关联的区域 (填入 Art 或 Gameplay 数据层均可，底层会自动识别)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MapTravel")
	TObjectPtr<UDataLayerAsset> TargetZone;

	// ==============================================================================
	// 碰撞事件 (Collision Events)
	// ==============================================================================
protected:
	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);
};
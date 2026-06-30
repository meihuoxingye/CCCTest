// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MyLevelTransitionActor.generated.h"

class UBoxComponent;

/**
 * 放置在 2.5D 场景边缘的无缝传送门
 * 归属于 MapTravel 模块，纯空间触发，肉身碰撞即刻跨越关卡
 */
UCLASS()
class CCC_API AMyLevelTransitionActor : public AActor
{
	GENERATED_BODY()

	// ==============================================================================
	// 生命周期与初始化 (Lifecycle & Initialization)
	// ==============================================================================
public:
	AMyLevelTransitionActor();

protected:
	virtual void BeginPlay() override;

	// ==============================================================================
	// 组件与属性 (Components & Properties)
	// ==============================================================================
protected:
	// 空间感应碰撞盒
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LevelTravel")
	TObjectPtr<UBoxComponent> TriggerBox;

	// 目标关卡名称 (必须与真实 Map 资产名称一模一样)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LevelTravel")
	FName TargetLevelName;

	// 这个特定过渡演出（如进特定区域）对应的专属 UI
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LevelTravel")
	TSoftClassPtr<class UUserWidget> TransitionSpecificUI;

	// 【新增】：该传送门专属的人工最小黑屏/UI等待时间 (秒)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LevelTravel", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MinimumLoadingTime = 1.5f;

	// ==============================================================================
	// 内部状态锁 (Internal State Locks)
	// ==============================================================================
private:
	// 【核心防御】：物理门控，防止单帧多次重入
	UPROPERTY()
	bool bHasTriggered = false;

	// ==============================================================================
	// 碰撞触发逻辑 (Collision Events)
	// ==============================================================================
protected:
	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);
};
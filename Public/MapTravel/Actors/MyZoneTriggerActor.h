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

protected:
	virtual void BeginPlay() override;

	// ==============================================================================
	// 组件与属性 (Components & Properties)
	// ==============================================================================
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trigger")
	TObjectPtr<UBoxComponent> CollisionComponent;

	// 触发器关联的区域 (填入 Art 或 Gameplay 数据层均可，底层会自动识别)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MapTravel")
	TObjectPtr<UDataLayerAsset> TargetZone;

	// 【你要求的过场配置】：区域专属过渡 UI
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MapTravel")
	TSoftClassPtr<class UUserWidget> TransitionSpecificUI;

	// 【你要求的时间等待】：强制等待时间 (秒)。
	// 设为 0：静默无缝流送（同一关卡不同区域）。
	// 大于 0：锁死输入，拉起黑屏 UI 进行掩护等待（同一地图不同关卡）。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MapTravel", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float WaitTime = 0.0f;

	// 静默流送时的防抖冷却时间 (秒)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MapTravel", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float TriggerCooldown = 1.0f;

	// ==============================================================================
	// 内部防抖状态锁 (Internal Debounce State)
	// ==============================================================================
private:
	// 用于静默刷新的循环防抖
	UPROPERTY()
	bool bIsOnCooldown = false;

	// 用于同地图硬切换的绝对死锁 (触发一次即失效)
	UPROPERTY()
	bool bHasTriggeredHardTravel = false;

	FTimerHandle CooldownTimerHandle;

	UFUNCTION()
	void ResetTriggerCooldown();

	// ==============================================================================
	// 碰撞事件 (Collision Events)
	// ==============================================================================
protected:
	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);
};
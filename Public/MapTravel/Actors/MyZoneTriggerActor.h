// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UI/Transition/MyTransitionWidgetBase.h"
#include "MyZoneTriggerActor.generated.h"

UCLASS()
class CCC_API AMyZoneTriggerActor : public AActor
{
	GENERATED_BODY()

	// ==============================================================================
	// 核心生命周期与组件 (Core Lifecycle & Components)
	// ==============================================================================
public:
	AMyZoneTriggerActor();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UBoxComponent* TriggerBox;


	// ==============================================================================
	// 转场触发逻辑 (Transition Trigger Logic)
	// ==============================================================================
protected:
	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);


	// ==============================================================================
	// 转场视觉配置 (Transition Visual Settings)
	// ==============================================================================
protected:
	// 同地图内的目标传送位置和朝向
	// 【注意】：表现层已彻底数据驱动化！UI蓝图与时间请去 UMyGameInstance 的全局字典中统一配置！
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition Settings", meta = (MakeEditWidget = true))
	FTransform TargetZoneTransform;
};
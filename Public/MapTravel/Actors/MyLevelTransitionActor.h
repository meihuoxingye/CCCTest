// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UI/Transition/MyTransitionWidgetBase.h"
#include "MyLevelTransitionActor.generated.h"

UCLASS()
class CCC_API AMyLevelTransitionActor : public AActor
{
	GENERATED_BODY()

	// ==============================================================================
	// 核心生命周期与组件 (Core Lifecycle & Components)
	// ==============================================================================
public:
	AMyLevelTransitionActor();

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
	// 跨地图漫游的目标地图名称
	// 【注意】：表现层已彻底数据驱动化！UI蓝图与时间请去 UMyGameInstance 的全局字典中统一配置！
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition Settings")
	FName TargetLevelName;
};
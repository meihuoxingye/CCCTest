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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition Settings", meta = (MakeEditWidget = true))
	FTransform TargetZoneTransform;

	// 离开当前区域时的闭合动画（如：纯黑硬切、眼皮闭上）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition Settings")
	TSoftClassPtr<class UMyTransitionWidgetBase> ScreenOffUIClass;

	// 闭合动画耗时
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition Settings", meta = (ClampMin = "0.1"))
	float ScreenOffDuration = 0.5f;

	// 抵达新区域时的加载或展示界面（例如：“发现新区域：黑龙巢穴”的霸气大字 UI）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition Settings")
	TSoftClassPtr<class UMyTransitionWidgetBase> AreaPresentationUIClass;

	// 展示 UI 至少停留的展示时间
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition Settings", meta = (ClampMin = "0.1"))
	float MinDisplayTime = 1.5f;
};
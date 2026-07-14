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

	// 反射标记：在所有蓝图和细节面板中仅可见、只读，防止美术误删触发器组件
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UBoxComponent* TriggerBox;


	// ==============================================================================
	// 转场触发逻辑 (Transition Trigger Logic)
	// ==============================================================================
protected:

	// 反射标记：动态重叠事件的回调函数必须加 UFUNCTION，否则无法注册到委托中
	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);


	// ==============================================================================
	// 转场视觉配置 (Transition Visual Settings)
	// ==============================================================================
protected:

	// 反射标记：允许在任何放置于场景中的实例或蓝图默认值中自由配置目标地图
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition Settings")
	FName TargetLevelName;
};
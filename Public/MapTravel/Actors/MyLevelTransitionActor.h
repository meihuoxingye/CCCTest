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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition Settings")
	FName TargetLevelName;

	// 离开当前地图时的闭合动画（例如：黑幕合拢、眼皮闭上、电视熄屏）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition Settings")
	TSoftClassPtr<class UMyTransitionWidgetBase> ScreenOffUIClass;

	// 闭合动画需要播放的时间。时间一到，旧世界瞬间毁灭。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition Settings", meta = (ClampMin = "0.1"))
	float ScreenOffDuration = 0.5f;

	// 落地新地图时的加载界面（例如：带有提示文字的擦除 UI，可继承为带有假进度条的子类）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition Settings")
	TSoftClassPtr<class UMyTransitionWidgetBase> LoadingScreenUIClass;

	// 落地新世界后，加载UI最小的滞留时间（供进度条跑动画或让玩家看清文字）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition Settings", meta = (ClampMin = "0.5"))
	float MinLoadingTime = 1.5f;
};
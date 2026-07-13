// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MyZoneTriggerActor.generated.h"

class UDataLayerAsset;

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
	// 数据层流送逻辑 (Data Layer Streaming Logic)
	// ==============================================================================
protected:
	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	// 【流送探针 / 区域身份证】：
	// 这里的 AssociatedDataLayer 只需要填入该区域的 Art层 或者 Gameplay层（二选一即可）。
	// 它不是用来单独加载的，而是作为一把“钥匙”或“身份证”传给 MyMapTravelSubsystem。
	// 子系统拿到这把钥匙后，会去全图注册的 ZoneSequence（双轨配对字典）里查表，
	// 瞬间就能定位玩家现在踩在几号区域，并自动同时激活该区域对应的 Art 和 Gameplay 两个数据层！
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming")
	UDataLayerAsset* AssociatedDataLayer;
};
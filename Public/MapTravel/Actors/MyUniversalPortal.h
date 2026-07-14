// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MyUniversalPortal.generated.h"

class UBoxComponent;
class UTeleportRoute;

UCLASS()
class CCC_API AMyUniversalPortal : public AActor
{
	GENERATED_BODY()

	// ==============================================================================
	// 核心生命周期与组件 (Core Lifecycle & Components)
	// ==============================================================================
public:

	AMyUniversalPortal();

protected:

	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* TriggerBox;


	// ==============================================================================
	// 传送发射配置 (Teleport Emitting Configuration)
	// ==============================================================================
public:

	// 发射路由：策划只需拖入目标路由资产，该传送门自动绑定物理路线
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Teleport")
	UTeleportRoute* EmittingRoute;

protected:

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
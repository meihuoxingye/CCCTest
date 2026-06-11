// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/MyInteractableInterface.h"
#include "MySaveStationActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

// ==============================================================================
// 物理存档终端 (Physical Save Station Actor)
// ==============================================================================

UCLASS()
class CCC_API AMySaveStationActor : public AActor, public IMyInteractableInterface
{
	GENERATED_BODY()

public:
	AMySaveStationActor();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> StationMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> TriggerBox;

public:
	// ==============================================================================
	// 接口规范实现 (Interface Implementation)
	// ==============================================================================

	virtual void Interact_Implementation(class ACharacter* Interactor) override;

	virtual FText GetInteractPrompt_Implementation() const override;

	virtual int32 GetInteractionPriority_Implementation() const override;
};
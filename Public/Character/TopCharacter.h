// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BaseCharacter.h"
#include "TopCharacter.generated.h"

UCLASS()
class CCC_API ATopCharacter : public ABaseCharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ATopCharacter();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class UMyMovementControlComponent> MMCComponent;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

private:


};

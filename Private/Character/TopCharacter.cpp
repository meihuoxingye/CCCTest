// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/TopCharacter.h"
#include "Component/MovementControl/MyMovementControlComponent.h"

// Sets default values
ATopCharacter::ATopCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	MMCComponent = CreateDefaultSubobject<UMyMovementControlComponent>(TEXT("MyMovementControlComponent"));
}

// Called when the game starts or when spawned
void ATopCharacter::BeginPlay()
{
	Super::BeginPlay();

}

// Called every frame
void ATopCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void ATopCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}


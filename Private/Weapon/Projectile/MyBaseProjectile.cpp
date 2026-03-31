// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon/Projectile/MyBaseProjectile.h"

// Sets default values
AMyBaseProjectile::AMyBaseProjectile()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AMyBaseProjectile::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AMyBaseProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}


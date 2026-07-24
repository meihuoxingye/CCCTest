// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon/MyWeaponBase.h"
#include "Component/CombatSystem/MyWeaponDataAsset.h"

// 使用静态网格体的独有函数
#include "Components/StaticMeshComponent.h"

// Sets default values
AMyWeaponBase::AMyWeaponBase()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
		// 关闭 Tick，追求极致性能
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

}

UStaticMeshComponent* AMyWeaponBase::GetWeaponMuzzleComponent() const
{
	// 静态网格体也有插槽
	return FindComponentByClass<UStaticMeshComponent>();
}

// Called when the game starts or when spawned
void AMyWeaponBase::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AMyWeaponBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}
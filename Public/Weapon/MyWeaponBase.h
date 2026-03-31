// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MyWeaponBase.generated.h"

UCLASS()
class CCC_API AMyWeaponBase : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AMyWeaponBase();

	// 提供获取武器数据资产配置的方法
	FORCEINLINE class UMyWeaponDataAsset* GetWeaponConfig() const { return WeaponConfig; }

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// 添加武器数据资产配置
	UPROPERTY(EditAnywhere, Category = "Combat Config")
	TObjectPtr<class UMyWeaponDataAsset> WeaponConfig;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};

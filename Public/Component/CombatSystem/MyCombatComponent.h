// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MyCombatComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class CCC_API UMyCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UMyCombatComponent();

	// 在控制器里调用的唯一入口
	void ExecuteAttack();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	// 添加武器数据资产配置
	UPROPERTY(EditAnywhere, Category = "Combat Config")
	TObjectPtr<class UMyWeaponDataAsset> WeaponConfig;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	// 执行 射线检测
	void PerformHitscan();
	// 召唤抛射物实体
	void SpawnProjectile();

	// 缓存组件拥有者的指针
	UPROPERTY()
	TObjectPtr<class ACharacter> CachedOwner;
};

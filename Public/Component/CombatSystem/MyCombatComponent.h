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

	// 切换为当前使用武器的接口，由 Controller 或 Character 调用
	void SwitchToActiveWeapon(class AMyWeaponBase* NewWeapon);

protected:
	// Called when the game starts
	virtual void BeginPlay() override;



public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	// 执行 射线检测
	void PerformHitscan(class UMyWeaponDataAsset* Config);
	// 召唤抛射物实体
	void SpawnProjectile(class UMyWeaponDataAsset* Config);

	// 缓存组件拥有者的指针
	UPROPERTY()
	TObjectPtr<class ACharacter> CachedOwner;

	// 缓存当前使用的武器
	UPROPERTY()
	TObjectPtr<class AMyWeaponBase> CachedActiveWeapon;
};

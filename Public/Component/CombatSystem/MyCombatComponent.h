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

	// 为拥有该组件的角色生成默认武器
	// 由基础角色调用
	void SpawnDefaultWeapon();
	// 将生成的默认武器吸附到角色的骨骼插槽上
	void AttachWeaponToSocket(class AMyWeaponBase* SpawnedWeapon);

protected:
	// Called when the game starts
	virtual void BeginPlay() override;



public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	// 执行 射线检测
	void PerformHitscan();
	// 召唤抛射物实体
	void SpawnProjectile();
	// 将 NewWeapon 切换为当前使用武器，并获取武器的网格与数据资产配置的接口，然后缓存它们
	// 设置为布尔值，缓存失败返回 false，用于在其他函数里进行条件判断
	bool SwitchToActiveWeapon(class AMyWeaponBase* NewWeapon);


	// 缓存组件拥有者的指针
	UPROPERTY()
	TObjectPtr<class ABaseCharacter> CachedOwner;

	// 缓存当前使用的武器
	UPROPERTY()
	TObjectPtr<class AMyWeaponBase> CachedActiveWeapon;

	// 缓存当前使用的武器的数据资产配置
	// 获取数据资产配置的 GetWeaponConfig 被 const 保护，这里也要加 const
	// 指针+const，保护的是指针指向的数据，不影响设置指针指向哪
	UPROPERTY()
	TObjectPtr<const class UMyWeaponDataAsset> CachedConfig;

	// 缓存当前使用的武器的网格
	// 静态网格也能使用插槽，且性能更好
	UPROPERTY()
	TObjectPtr<const class UStaticMeshComponent> CachedWeaponMesh;

	// 缓存子弹子系统指针
	UPROPERTY()
	TObjectPtr<class UMyBulletSubsystem> CachedBulletSubsystem;
};

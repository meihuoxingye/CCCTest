// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MyCombatComponent.generated.h"


UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class CCC_API UMyCombatComponent : public UActorComponent
{
	GENERATED_BODY()


	// ==============================================================================
	// 核心生命周期 (Core Lifecycle)
	// ==============================================================================
public:
	// Sets default values for this component's properties
	UMyCombatComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;


	// ==============================================================================
	// 武器生成与装配 (Weapon Spawn & Assembly)
	// ==============================================================================
public:
	// 为拥有该组件的角色生成默认武器
	// 由基础角色调用
	void SpawnDefaultWeapon();
	// 将生成的默认武器吸附到角色的骨骼插槽上
	void AttachWeaponToSocket(class AMyWeaponBase* SpawnedWeapon);

private:
	// 将 NewWeapon 切换为当前使用武器，并获取武器的网格与数据资产配置的接口，然后缓存它们
	// 设置为布尔值，缓存失败返回 false，用于在其他函数里进行条件判断
	bool SwitchToActiveWeapon(class AMyWeaponBase* NewWeapon);


	// ==============================================================================
	// 战斗指令与状态 (Combat Commands & State)
	// ==============================================================================
public:
	// 外部调用，执行开火逻辑
	void ExecuteAttack();
	// 外部调用，将组件注册进射击子系统的开火冷却名单数组中
	void StartWeaponFire();
	// 外部调用，将组件从射击子系统的开火冷却名单数组中移除
	void StopWeaponFire();


	// ==============================================================================
	// 底层开火系统实现 (Low-Level Firing Implementation)
	// ==============================================================================
private:
	// 执行射线检测
	void PerformHitscan();
	// 召唤抛射物实体
	void SpawnProjectile();


	// ==============================================================================
	// 辅助与缓存工具 (Utilities & Caching)
	// ==============================================================================
protected:
	// 缓存组件拥有者的玩家或 AI 控制器；
	// 组件的 BeginPlay 执行时，角色可能还没有被控制器控制，所以不直接在 BeginPlay 里缓存
	void CachedController();

private:
	// 缓存组件拥有者的指针
	UPROPERTY()
	TObjectPtr<class ABaseCharacter> CachedOwner;

	// 缓存组件拥有者的控制器的指针
	UPROPERTY()
	TObjectPtr<class AController> CachedOwnerController;

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

	// 缓存当前使用的武器的枪口插槽
	FName CachedMuzzleSocket;

	// 缓存子弹子系统指针
	UPROPERTY()
	TObjectPtr<class UMyBulletSubsystem> CachedBulletSubsystem;

	// 记录是否已经尝试去抓过控制器了
	bool bControllerChecked = false;

	// 缓存玩家控制器（如果是 AI，则自动为 nullptr）
	UPROPERTY()
	TObjectPtr<class APlayerController> CachedPlayerController;

	// 缓存玩家相机管理器（如果是 AI，则自动为 nullptr）
	UPROPERTY()
	TObjectPtr<class APlayerCameraManager> CachedCameraManager;

	// 缓存 AI 控制器（如果是 玩家，则自动为 nullptr）
	UPROPERTY()
	TObjectPtr<class AAIController> CachedAIController;
};
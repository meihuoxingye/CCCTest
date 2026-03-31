// Fill out your copyright notice in the Description page of Project Settings.


#include "Component/CombatSystem/MyCombatComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Weapon/Projectile/MyBaseProjectile.h"
#include "Weapon/AsyncLineTraceBullet/MyBulletSubsystem.h"
#include "Weapon/MyWeaponBase.h"
#include "Component/CombatSystem/MyWeaponDataAsset.h"

// Sets default values for this component's properties
UMyCombatComponent::UMyCombatComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}

void UMyCombatComponent::ExecuteAttack()
{
	// 未设置武器数据资产配置或拥有组件者不是 Charater
	if (!CachedActiveWeapon || !CachedOwner) return;

	// 2. 核心：在判定时直接引用武器携带的数据资产
	// 这就是你要求的“补上引用”，保证了逻辑始终基于当前武器的最新配置
	UMyWeaponDataAsset* Config = CachedActiveWeapon->GetWeaponConfig();
	if (!Config) return;

	// 根据数据资产配置决定执行线迹追踪还是生成抛射物
	if (Config->FireType == EWeaponFireType::Hitscan)
	{
		PerformHitscan(Config);
	}
	else
	{
		SpawnProjectile(Config);
	}
}

void UMyCombatComponent::SwitchToActiveWeapon(AMyWeaponBase* NewWeapon)
{
	CachedActiveWeapon = NewWeapon;
}

// Called when the game starts
void UMyCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	// 缓存组件拥有者
	CachedOwner = Cast<ACharacter>(GetOwner());
}


// Called every frame
void UMyCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UMyCombatComponent::PerformHitscan(UMyWeaponDataAsset* Config)
{
	FVector MuzzleLoc = CachedOwner->GetMesh()->GetSocketLocation(Config->MuzzleSocketName);
	FVector Dir = CachedOwner->GetActorForwardVector();

	// 获取子系统并开火
	UMyBulletSubsystem* BulletSubsystem = GetWorld()->GetSubsystem<UMyBulletSubsystem>();
	if (BulletSubsystem)
	{
		// 传参：谁开的枪，哪里开的，方向，速度（从 DataAsset 拿），寿命
		BulletSubsystem->FireBullet(GetOwner(), MuzzleLoc, Dir, Config->BulletSpeed, 5.0f);
	}
}

void UMyCombatComponent::SpawnProjectile(UMyWeaponDataAsset* Config)
{
	if (!Config->ProjectileClass) return;

	FVector Loc = CachedOwner->GetMesh()->GetSocketLocation(Config->MuzzleSocketName);
	FRotator Rot = CachedOwner->GetActorRotation();

	FActorSpawnParameters Params;
	Params.Owner = GetOwner();
	Params.Instigator = CachedOwner;

	// 生成那个“带着原生抛射物组件”的子弹，生成后逻辑交给子弹自己
	GetWorld()->SpawnActor<AMyBaseProjectile>(Config->ProjectileClass, Loc, Rot, Params);
}
// Fill out your copyright notice in the Description page of Project Settings.


#include "Component/CombatSystem/MyCombatComponent.h"
// 基础角色类
#include "Character/BaseCharacter.h"
// 系统函数库，可用来调试打印
#include "Kismet/KismetSystemLibrary.h"
// 基础抛射物类
#include "Weapon/Projectile/MyBaseProjectile.h"
// 子弹子系统类
#include "Weapon/AsyncLineTraceBullet/MyBulletSubsystem.h"
// 武器基类
#include "Weapon/MyWeaponBase.h"
// 武器数据资产配置类
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

	if (!CachedConfig) return;

	// 根据数据资产配置决定执行线迹追踪还是生成抛射物
	if (CachedConfig->FireType == EWeaponFireType::Hitscan)
	{
		PerformHitscan();
	}
	else
	{
		SpawnProjectile();
	}
}

void UMyCombatComponent::SwitchToActiveWeapon(AMyWeaponBase* NewWeapon)
{
	// 缓存当前使用武器
	CachedActiveWeapon = NewWeapon;

	if (CachedActiveWeapon)
	{
		// 缓存当前武器网格
		CachedWeaponMesh = CachedActiveWeapon->GetWeaponMuzzleComponent();

		// 缓存武器携带的数据资产配置
		CachedConfig = CachedActiveWeapon->GetWeaponConfig();
	}
}

void UMyCombatComponent::SpawnDefaultWeapon()
{
	// 配置生成参数
	// 定义一个生成参数清单，它的大多数值都是空的，所以需要手动填上最重要的两项
	FActorSpawnParameters SpawnParams;
	// 这把枪属于谁
	SpawnParams.Owner = CachedOwner;
	// 谁发起的这次行为
	SpawnParams.Instigator = CachedOwner;

	// 生成武器实体
	AMyWeaponBase* SpawnedWeapon = GetWorld()->SpawnActor<AMyWeaponBase>(CachedOwner->GetDefaultWeaponClass(), SpawnParams);

	// 将生成的武器切换为当前使用武器，并缓存相关数据
	SwitchToActiveWeapon(SpawnedWeapon);

	// 检查是否忘记设置插槽名
	if (CachedConfig->WeaponSocketName.IsNone())
	{
		// 在控制台和日志中输出警告，%s 会替换为当前武器数据资产的名字
		UE_LOG(LogTemp, Warning, TEXT("武器数据资产 [%s] 忘记设置 WeaponSocketName 了！"), *CachedConfig->GetName());

		// 可选：在此处直接返回，防止子弹从角色原点发射
		return;
	}

	// 吸附到角色插槽上
	AttachWeaponToSocket(CachedActiveWeapon);
}

void UMyCombatComponent::AttachWeaponToSocket(AMyWeaponBase* SpawnedWeapon)
{
	CachedActiveWeapon->AttachToComponent(
		CachedOwner->GetMesh(),
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		CachedConfig->WeaponSocketName
	);
}

// Called when the game starts
void UMyCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	// 缓存组件拥有者
	CachedOwner = Cast<ABaseCharacter>(GetOwner());

	// 缓存子弹子系统
	CachedBulletSubsystem = GetWorld()->GetSubsystem<UMyBulletSubsystem>();

	// 缓存当前使用武器与武器网格
	// SwitchToActiveWeapon(AMyWeaponBase* NewWeapon)
}


// Called every frame
void UMyCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UMyCombatComponent::PerformHitscan()
{
	// 未识别到武器网格则退出
	if (!CachedWeaponMesh) return;

	// 检查是否忘记设置插槽名
	if (CachedConfig->MuzzleSocketName.IsNone())
	{
		// 在控制台和日志中输出警告，%s 会替换为当前武器数据资产的名字
		// 编译器期望接收到的是一个底层的 C 风格字符指针，所以要加*
		UE_LOG(LogTemp, Warning, TEXT("武器数据资产 [%s] 忘记设置 MuzzleSocketName 了！"), *CachedConfig->GetName());

		// 可选：在此处直接返回，防止子弹从角色原点发射
		return;
	}

	// 射线检测起点，某插槽位置
	const FVector MuzzleLoc = CachedWeaponMesh->GetSocketLocation(CachedConfig->MuzzleSocketName);
	// 获取插槽旋转，然后用 Vector() 将欧拉角（旋转）转为前向向量
	const FVector Dir = CachedWeaponMesh->GetSocketRotation(CachedConfig->MuzzleSocketName).Vector();

	// 发射子弹
	if (CachedBulletSubsystem)
	{
		// 传参：谁开的枪，哪里开的，方向，速度（从 DataAsset 拿），寿命
		CachedBulletSubsystem->FireBullet(CachedOwner, MuzzleLoc, Dir, CachedConfig->BulletSpeed, CachedConfig->BulletLifespan);
	}
}


// 待修改
void UMyCombatComponent::SpawnProjectile()
{
	if (!CachedConfig->ProjectileClass) return;

	// 锁定生成位置和旋转，这些值计算出来后本帧内是固定的
	const FVector Loc = CachedOwner->GetMesh()->GetSocketLocation(CachedConfig->MuzzleSocketName);
	const FRotator Rot = CachedOwner->GetActorRotation();

	FActorSpawnParameters Params;
	Params.Owner = GetOwner();
	Params.Instigator = CachedOwner;

	// 生成那个“带着原生抛射物组件”的子弹，生成后逻辑交给子弹自己
	GetWorld()->SpawnActor<AMyBaseProjectile>(CachedConfig->ProjectileClass, Loc, Rot, Params);
}
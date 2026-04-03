// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MyWeaponDataAsset.generated.h"


// 自定义枚举类，确定是子弹是射线检测还是物理抛射物
UENUM(BlueprintType)
enum class EWeaponFireType : uint8
{
    // 异步线迹追踪
	Hitscan    UMETA(DisplayName = "AsyncLineTrace"),
    // 抛射物
	Projectile UMETA(DisplayName = "Projectile")
};

// 自定义枚举类，确定枪械射击模式
UENUM(BlueprintType)
enum class EFireMode : uint8
{
    // 半自动/单发点射
    SemiAuto    UMETA(DisplayName = "Semi-Auto"),
    // 三发点射/短点射
    Burst   UMETA(DisplayName = "Burst"),
    // 全自动/连射
    FullAuto    UMETA(DisplayName = "Full-Auto")
};


UCLASS(BlueprintType)
class CCC_API UMyWeaponDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
    // 子弹类型，枚举类
    UPROPERTY(EditAnywhere, Category = "Logic")
    EWeaponFireType FireType = EWeaponFireType::Hitscan;

    // 如果是抛射物模式，指定具体的抛射子弹类，如果不是，则不隐藏此栏
    // 下拉框，选择 C++ 或蓝图类
    UPROPERTY(EditAnywhere, Category = "Projectile", meta = (EditCondition = "FireType == EWeaponFireType::Projectile", EditConditionHides))
    TSubclassOf<class AMyBaseProjectile> ProjectileClass;

    // 枪械射击模式，枚举类
    UPROPERTY(EditAnywhere, Category = "Logic")
    EFireMode FireMode = EFireMode::SemiAuto;


    // 伤害
    UPROPERTY(EditAnywhere, Category = "Stats")
    float Damage = 20.f;
    // 射程
    UPROPERTY(EditAnywhere, Category = "Stats")
    float Range = 500.f;
    // 射击间隔
    UPROPERTY(EditAnywhere, Category = "Stats")
    float RefireTime = 1.f;
    // 子弹速度
    UPROPERTY(EditAnywhere, Category = "Stats")
    float BulletSpeed = 1000.f;
    // 子弹生命周期
    UPROPERTY(EditAnywhere, Category = "Stats")
    float BulletLifespan = 5.f;


    // 设置枪口插槽
    // 当前可填写 Muzzle_Socket
    UPROPERTY(EditAnywhere, Category = "Visuals")
    FName MuzzleSocketName;

    // 设置角色身上的武器装备插槽
    // 当前可填写 WeaponSocket
    UPROPERTY(EditAnywhere, Category = "Visuals")
    FName WeaponSocketName;

};

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


UCLASS(BlueprintType)
class CCC_API UMyWeaponDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
    // 子弹类型，枚举类
    UPROPERTY(EditAnywhere, Category = "Logic")
    EWeaponFireType FireType = EWeaponFireType::Hitscan;

    // 如果是抛射物模式，指定具体的抛射子弹类，如果不是，则不会启用此栏
    // 下拉框，选择 C++ 或蓝图类
    UPROPERTY(EditAnywhere, Category = "Projectile", meta = (EditCondition = "FireType == EWeaponFireType::Projectile"))
    TSubclassOf<class AMyBaseProjectile> ProjectileClass;

    // 伤害
    UPROPERTY(EditAnywhere, Category = "Stats")
    float Damage = 20.f;
    // 射程
    UPROPERTY(EditAnywhere, Category = "Stats")
    float Range = 500.f;
    // 子弹速度
    UPROPERTY(EditAnywhere, Category = "Stats")
    float BulletSpeed = 1000.f;

    // 选择枪口插槽，默认为 Muzzle_Socket
    UPROPERTY(EditAnywhere, Category = "Visuals")
    FName MuzzleSocketName = FName("Muzzle_Socket");
};

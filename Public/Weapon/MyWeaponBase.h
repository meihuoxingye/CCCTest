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

	// 对外提供获取武器数据资产配置的方法
	// 左边的 const：保护返回值
	// 右边的 const：外部执行此函数时，这个函数的本来的类的任何成员变量不会被修改
	FORCEINLINE const class UMyWeaponDataAsset* GetWeaponConfig() const { return WeaponConfig; }

	// 添加一个获取武器网格体的接口
	UFUNCTION(BlueprintCallable)
	virtual UStaticMeshComponent* GetWeaponMuzzleComponent() const;

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
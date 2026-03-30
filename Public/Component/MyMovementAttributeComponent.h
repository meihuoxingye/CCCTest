// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MyMovementAttributeComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class CCC_API UMyMovementAttributeComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UMyMovementAttributeComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	// 存储指向数据资产的指针
	// Configuration 配置
	UPROPERTY(EditAnywhere, Category = "Movement Data Asset")
	TObjectPtr<class UMyMovementDataAsset> MovementConfig;
    // 同步自定义移动属性
    void SyncMovementProperties();

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

		
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BaseCharacter.h"

// 定义了 FInputActionValue 结构体
// 专门用来装载按键、鼠标位移或手柄摇杆产生的具体数值
#include "InputActionValue.h"
#include "WorldPartition/DataLayer/DataLayerInstance.h"

#include "TopCharacter.generated.h"

UCLASS()
class CCC_API ATopCharacter : public ABaseCharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ATopCharacter();


	// ==============================================================================
	// 核心生命周期 (Core Lifecycle)
	// ==============================================================================
public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 【响应式架构】：监听 physical/solid assets 数据层状态改变的回调函数
	UFUNCTION()
	void OnDataLayerStateChanged(const UDataLayerInstance* DataLayer, EDataLayerRuntimeState State);

	// 自我清理的执行逻辑
	void ExecuteSelfSanitization();


	// ==============================================================================
	// 角色组件 (Character Components)
	// ==============================================================================
protected:
	// 自定义移动控制组件
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class UMyMovementControlComponent> MMCComponent;


	// ==============================================================================
	// 交互统筹系统 (Interaction Management System)
	// ==============================================================================
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<class USphereComponent> InteractionSphere;

	// 缓存当前处于范围内的所有合法物体
	TArray<TWeakObjectPtr<AActor>> InteractableActorsInRange;

	UFUNCTION()
	void OnInteractSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnInteractSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
	// 核心算法：提取距离最近、朝向最准且优先级最高的目标
	AActor* GetClosestInteractableActor();


	// ==============================================================================
	// 玩家输入与行为绑定 (Player Input & Actions)
	// ==============================================================================
public:
	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// 暴露接口，让大管家来拿我的输入配置
	FORCEINLINE class UInputMappingContext* GetDefaultMappingContext() const { return DefaultMappingContext; }

private:
	// ===================== 【增强输入动作 (原属于控制器)】 =====================
	// 【虚幻5.8】：输入映射上下文，搭起物理按键和 Action 之间的桥梁
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<class UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<class UInputAction> MoveAction;
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<class UInputAction> JumpAction;
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<class UInputAction> AttackAction;
	// 【新增】：交互按键动作
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<class UInputAction> InteractAction;

	/** Input Callback Functions*/
	void Move(const FInputActionValue& InputActionValue);
	void Attack();
	void AttackEnd();
	// 【新增】：交互触发回调
	void OnInteractKeyPressed();

	// ==============================================================================
	// 联机底层探针 (Network Probes)
	// ==============================================================================
public:
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_Controller() override;
};
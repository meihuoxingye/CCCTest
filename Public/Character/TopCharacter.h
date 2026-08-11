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

	// 重写底层销毁事件，作为生命周期的最后一道防线
	virtual void Destroyed() override;

private:
	// 【新增】：统一清理函数，防重入与内存泄漏
	void PerformCleanup();

	// 【新增】：清理状态锁
	bool bHasDoneCleanup = false;


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
	// 交互探测球组件 (雷达)：用于在 3D 空间中物理捕获周围的可交互物体
	// VisibleAnywhere 允许在编辑器细节面板中查看并修改其半径等内部属性，BlueprintReadOnly 确保组件指针本身的内存基址不会被蓝图意外覆写
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<class USphereComponent> InteractionSphere;

	// 缓存当前处于范围内的所有合法物体
	TArray<TWeakObjectPtr<AActor>> InteractableActorsInRange;

	// 物理重叠响应 (进入)：当任意动态物理对象穿入雷达范围时触发（内部执行接口校验与压栈）
	UFUNCTION()
	void OnInteractSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	// 物理重叠响应 (离开)：当任意动态物理对象脱离雷达范围时触发（内部执行防抖验证与出栈清理）
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
	// 本地控制钩子：在控制器成功附身且本地输入组件准备就绪后自动调用，用于挂载增强输入映射
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
	// 权威钩子：仅在服务器端触发，当玩家控制器 (PlayerController) 成功附身接管该肉体时调用
	virtual void PossessedBy(AController* NewController) override;

	// 【新增】：失去控制权时的原生钩子 (如玩家断线、跨图剥离或被强行夺舍时在服务器端触发)
	virtual void UnPossessed() override;

	// 客户端同步回调：仅在客机本地触发，当服务器下发的 Controller 灵魂指针完成网络同步时调用
	virtual void OnRep_Controller() override;
};
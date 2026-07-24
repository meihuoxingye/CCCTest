// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/MyInteractableInterface.h"
#include "MySaveStationActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

// ==============================================================================
// 物理存档终端 (Physical Save Station Actor)
// ==============================================================================
UCLASS()
class CCC_API AMySaveStationActor : public AActor, public IMyInteractableInterface
{
	GENERATED_BODY()

	// ==============================================================================
	// 核心生命周期与初始化 (Core Lifecycle & Initialization)
	// ==============================================================================
public:

	// 默认构造函数，用于配置基础组件与碰撞响应
	AMySaveStationActor();

	// ==============================================================================
	// 核心组件 (Core Components)
	// ==============================================================================
protected:

	// 静态网格体组件：呈现存档终端在世界中的 3D 视觉模型
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> StationMesh;

	// 盒体碰撞组件：定义玩家可触发交互的隐形空间边界
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> TriggerBox;

	// ==============================================================================
	// 接口规范实现 (Interface Implementation)
	// ==============================================================================
public:

	// 执行交互逻辑：玩家控制器通过此接口安全触发存档面板翻转
	// 发起者 ATopCharacter::OnInteractKeyPressed()
	virtual void Interact_Implementation(class ACharacter* Interactor) override;

	// 未实现
	// 获取交互提示：向 HUD 层的交互提示框提供渲染文本
	// 例如要显示：“按 E 键保存游戏”
	virtual FText GetInteractPrompt_Implementation() const override;

	// 获取交互优先级：多个可交互物件重叠时用于权重的裁决依据
	virtual int32 GetInteractionPriority_Implementation() const override;
};
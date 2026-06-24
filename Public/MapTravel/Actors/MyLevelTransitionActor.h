// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/MyInteractableInterface.h"
#include "MyLevelTransitionActor.generated.h"

/**
 * 放置在 2.5D 场景边缘的传送门或出口
 * 归属于 MapTravel 模块，触发无缝关卡切换与 LSP 状态流转
 */
UCLASS()
class CCC_API AMyLevelTransitionActor : public AActor, public IMyInteractableInterface
{
	GENERATED_BODY()

	// ==============================================================================
	// 生命周期与初始化 (Lifecycle & Initialization)
	// ==============================================================================
public:
	AMyLevelTransitionActor();

	// ==============================================================================
	// 属性配置 (Properties Configuration)
	// ==============================================================================
protected:
	// 目标关卡名称
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LevelTravel")
	FName TargetLevelName;

	// UI 提示词
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LevelTravel")
	FText TravelPromptText;

	// ==============================================================================
	// IMyInteractableInterface 接口实现 (Interactable Interface Implementation)
	// ==============================================================================
public:
	virtual void Interact_Implementation(class ACharacter* Interactor) override;
	virtual FText GetInteractPrompt_Implementation() const override;
	virtual int32 GetInteractionPriority_Implementation() const override;
};
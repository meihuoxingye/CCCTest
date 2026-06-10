// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MyInteractableInterface.generated.h"

UINTERFACE(MinimalAPI)
class UMyInteractableInterface : public UInterface
{
	GENERATED_BODY()
};

class CCC_API IMyInteractableInterface
{
	GENERATED_BODY()

public:
	// 触发交互的核心入口
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void Interact(class ACharacter* Interactor);

	// 获取屏幕提示词 (如 "按E 写入神经芯片")
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	FText GetInteractPrompt() const;

	// 获取交互优先级 (决定在一堆物体中谁被优先触发)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	int32 GetInteractionPriority() const;
};
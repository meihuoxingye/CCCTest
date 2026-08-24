// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "MyPlayerState.generated.h"

class UMyMapTravelStateComponent;

/**
 * 全局通用的玩家状态基类。
 * 纯净的载体：不写任何具体业务逻辑，只负责在底层 C++ 中强行焊死各种系统组件。
 */
UCLASS()
class CCC_API AMyPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AMyPlayerState();

	// 核心挂载：大一统传送状态组件
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|MapTravel")
	TObjectPtr<UMyMapTravelStateComponent> MapTravelComponent;
};
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MyMainMenuGameMode.generated.h"

/**
 * 主菜单专属 GameMode
 * 极简架构：不需要 Pawn，不需要专属 PlayerController，全权负责 UI 环境的搭建。
 */
UCLASS()
class CCC_API AMyMainMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

	// ==============================================================================
	// 生命周期与初始化 (Lifecycle & Initialization)
	// ==============================================================================
public:
	AMyMainMenuGameMode();

protected:
	virtual void BeginPlay() override;


	// ==============================================================================
	// 核心配置 (Core Configurations)
	// ==============================================================================
public:
	// 暴露给蓝图：主菜单 UI 类模板
	UPROPERTY(EditDefaultsOnly, Category = "MainMenu|UI")
	TSubclassOf<class UMyMainMenuWidget> MainMenuWidgetClass;

	// 暴露给蓝图：点击“开始新游戏”时要前往的第一关 (纯净开荒地图)
	// 使用软指针，防止硬引用导致在主菜单就强行把庞大的第一关加载进内存
	UPROPERTY(EditDefaultsOnly, Category = "MainMenu|Level")
	TSoftObjectPtr<UWorld> DefaultStartMap;
};
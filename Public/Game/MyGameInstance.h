// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Blueprint/UserWidget.h"
#include "MyGameInstance.generated.h"

class FMyPreLoadScreen;

/**
 * 全局游戏实例，负责剥离动态 UMG 灵魂并将其跨越生命周期托管
 */
UCLASS()
class CCC_API UMyGameInstance : public UGameInstance
{
	GENERATED_BODY()

	// ==============================================================================
	// 引擎生命周期 (Engine Lifecycle)
	// ==============================================================================
public:
	virtual void Init() override;
	virtual void Shutdown() override;

	// ==============================================================================
	// 物理层加载遮罩控制 (Physical Loading Screen Control)
	// ==============================================================================
public:
	// 动态接收大管家平移过来的过场 UI 资产
	UFUNCTION(BlueprintCallable, Category = "LoadingScreen")
	void ShowGlobalBlackScreen(TSoftClassPtr<UUserWidget> DynamicLoadingUI = nullptr);

	UFUNCTION(BlueprintCallable, Category = "LoadingScreen")
	void HideGlobalBlackScreen();

	// ==============================================================================
	// 内存生命周期管理 (Internal Memory Management)
	// ==============================================================================
private:
	UPROPERTY()
	UUserWidget* PersistentLoadingWidget = nullptr;

	TSharedPtr<FMyPreLoadScreen> ActivePreLoadScreen;
};
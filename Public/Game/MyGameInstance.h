// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Blueprint/UserWidget.h"
#include "Engine/TimerHandle.h"
#include "Engine/Engine.h" 
#include "MyGameInstance.generated.h"


// ==============================================================================
// 核心生命周期与组件 (Core Lifecycle & Components)
// ==============================================================================
UCLASS()
class CCC_API UMyGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;

	// ==============================================================================
	// 伪加载管线 UI 管理 (Fake Loading Pipeline UI)
	// ==============================================================================
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading")
	TSoftClassPtr<class UUserWidget> DefaultLoadingUIClass;

	// 旧的伪加载时间保留作为兜底，新增一个最小显示时间
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading")
	float MinUIShowDuration = 1.5f;

	// 【新增】：拉起关卡设计师配置的个性化熄屏 UI
	UFUNCTION(BlueprintCallable, Category = "Loading")
	void PlayScreenOffUI(TSoftClassPtr<class UUserWidget> ScreenOffUIClass);

	UPROPERTY(Transient)
	FName PendingTargetMapName;

	void ShowFakeLoadingScreen(TSoftClassPtr<class UUserWidget> CustomUI);

	UFUNCTION(BlueprintCallable, Category = "Loading")
	void HideFakeLoadingScreen();

	UFUNCTION()
	void HandleStartTravel(UWorld* CurrentWorld);

	UFUNCTION()
	void HandleEndTravel(UWorld* NewWorld);

private:
	TSharedPtr<class SWidget> PureFakeLoadingSlate;

	FTimerHandle FakeLoadingTimerHandle;

	TSharedPtr<class FBlackoutExtension, ESPMode::ThreadSafe> BlackoutExt;

	FBeginStreamingPauseDelegate BeginStreamingPauseDelegate;
	FEndStreamingPauseDelegate EndStreamingPauseDelegate;

	void OnBeginStreamingPause(FViewport* Viewport);
	void OnEndStreamingPause();

	// 【新增状态锁】：用于双轨制时间对齐
	double UIStartTime = 0.0;
	bool bEngineIsReady = false;
	bool bMinTimeElapsed = false;

	void CheckAndHideLoadingScreen();
};
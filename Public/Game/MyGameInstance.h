// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "UI/Transition/MyTransitionWidgetBase.h"
#include "Engine/TimerHandle.h"
#include "Engine/Engine.h" 
#include "MyGameInstance.generated.h"

// ==============================================================================
// 地图专属转场配置 (Map Transition Config)
// ==============================================================================
USTRUCT(BlueprintType)
struct FMapTransitionConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition|Outro")
	TSoftClassPtr<class UMyTransitionWidgetBase> ScreenOffUIClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition|Outro", meta = (ClampMin = "0.1"))
	float ScreenOffDuration = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition|Intro")
	TSoftClassPtr<class UMyTransitionWidgetBase> LoadingScreenUIClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition|Intro", meta = (ClampMin = "0.5"))
	float MinLoadingTime = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition|Intro", meta = (ClampMin = "0.0"))
	float HoldTimeAtFull = 0.5f;
};

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
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Loading|Registry")
	float SystemSafeDelay = 0.1f;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Loading|Registry")
	float DefaultIntroDelay = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading|Registry")
	TMap<FName, FMapTransitionConfig> MapTransitionRegistry;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading|Registry")
	FMapTransitionConfig DefaultTransitionConfig;

	UFUNCTION(BlueprintPure, Category = "Loading|Registry")
	FMapTransitionConfig GetMapTransitionConfig(FName MapName) const;

	UPROPERTY(Transient)
	FName PendingTargetMapName;

	UFUNCTION(BlueprintCallable, Category = "Loading")
	void PlayScreenOffUI(TSoftClassPtr<class UMyTransitionWidgetBase> ScreenOffUIClass, float InDuration);

	void ShowFakeLoadingScreen(TSoftClassPtr<class UMyTransitionWidgetBase> CustomUI);

	UFUNCTION(BlueprintCallable, Category = "Loading")
	void HideFakeLoadingScreen();

	UFUNCTION(BlueprintCallable, Category = "Loading")
	void FinalizeLoadingScreenRemoval();

	UFUNCTION()
	void HandleStartTravel(UWorld* CurrentWorld);

	UFUNCTION()
	void HandleEndTravel(UWorld* NewWorld);

	UFUNCTION(BlueprintCallable, Category = "Loading")
	FORCEINLINE bool IsEngineReady() const { return bEngineIsReady; }

private:
	UPROPERTY(Transient)
	UMyTransitionWidgetBase* ActiveTransitionUI;

	FTimerHandle FakeLoadingTimerHandle;
	FTimerHandle EngineReadyPollTimerHandle;

	TSharedPtr<class FBlackoutExtension, ESPMode::ThreadSafe> BlackoutExt;

	FBeginStreamingPauseDelegate BeginStreamingPauseDelegate;
	FEndStreamingPauseDelegate EndStreamingPauseDelegate;

	void OnBeginStreamingPause(FViewport* Viewport);
	void OnEndStreamingPause();

	void PollEngineReadyStatus();

	double UIStartTime = 0.0;
	bool bEngineIsReady = false;
	bool bMinTimeElapsed = false;

	// 【新增】：核心时间探针，专门记录 Persistent 进内存的绝对时间！
	double PersistentLevelLoadTime = 0.0;

	void CheckAndHideLoadingScreen();
};
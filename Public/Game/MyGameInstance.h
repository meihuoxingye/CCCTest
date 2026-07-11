// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Blueprint/UserWidget.h"
#include "Engine/TimerHandle.h"
#include "Engine/Engine.h" 
#include "Widgets/SCompoundWidget.h" // 声明 Slate 类所必需的头文件
#include "MyGameInstance.generated.h"

// ==============================================================================
// 内部纯 Slate 渐暗组件 (Internal Slate Fade Widget)
// ==============================================================================
class CCC_API SBlackFadeWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlackFadeWidget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	float CurrentAlpha;
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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading")
	TSoftClassPtr<class UUserWidget> DefaultLoadingUIClass;

	// 旧的伪加载时间保留作为兜底，新增一个最小显示时间
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading")
	float MinUIShowDuration = 1.5f;

	UPROPERTY(Transient)
	FName PendingTargetMapName;

	void StartBlackFade();
	void ShowFakeLoadingScreen(TSoftClassPtr<class UUserWidget> CustomUI);

	UFUNCTION(BlueprintCallable, Category = "Loading")
	void HideFakeLoadingScreen();

	UFUNCTION()
	void HandleStartTravel(UWorld* CurrentWorld);

	UFUNCTION()
	void HandleEndTravel(UWorld* NewWorld);

private:
	TSharedPtr<class SWidget> PureBlackFadeSlate;
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
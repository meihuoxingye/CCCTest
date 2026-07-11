// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
// 引入我们的转场基类
#include "UI/Transition/MyTransitionWidgetBase.h"
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
	// 【强制契约】：关卡设计师只能选继承了我们基类的 UI
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading")
	TSoftClassPtr<class UMyTransitionWidgetBase> DefaultLoadingUIClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading")
	float MinUIShowDuration = 1.5f;

	// 供子类或加载进度条提取使用的目标地图名
	UPROPERTY(Transient)
	FName PendingTargetMapName;

	// 【新增】：拉起关卡设计师定制的熄屏闭合 UI
	UFUNCTION(BlueprintCallable, Category = "Loading")
	void PlayScreenOffUI(TSoftClassPtr<class UMyTransitionWidgetBase> ScreenOffUIClass);

	void ShowFakeLoadingScreen(TSoftClassPtr<class UMyTransitionWidgetBase> CustomUI);

	UFUNCTION(BlueprintCallable, Category = "Loading")
	void HideFakeLoadingScreen();

	// 【暴露给转场基类调用的终极粉碎函数】
	UFUNCTION(BlueprintCallable, Category = "Loading")
	void FinalizeLoadingScreenRemoval();

	UFUNCTION()
	void HandleStartTravel(UWorld* CurrentWorld);

	UFUNCTION()
	void HandleEndTravel(UWorld* NewWorld);

private:
	// 【完美保命符】：死死抓住新世界的加载 UI，防 GC 回收
	UPROPERTY(Transient)
	UMyTransitionWidgetBase* ActiveTransitionUI;

	FTimerHandle FakeLoadingTimerHandle;
	TSharedPtr<class FBlackoutExtension, ESPMode::ThreadSafe> BlackoutExt;

	FBeginStreamingPauseDelegate BeginStreamingPauseDelegate;
	FEndStreamingPauseDelegate EndStreamingPauseDelegate;

	void OnBeginStreamingPause(FViewport* Viewport);
	void OnEndStreamingPause();

	double UIStartTime = 0.0;
	bool bEngineIsReady = false;
	bool bMinTimeElapsed = false;

	void CheckAndHideLoadingScreen();
};
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Blueprint/UserWidget.h"
#include "MyGameInstance.generated.h"

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
	// 加载屏配置与动态接管 (Loading Screen Config & Dynamic Takeover)
	// ==============================================================================
public:
	// 默认的保底加载 UI
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LoadingScreen")
	TSoftClassPtr<class UUserWidget> DefaultLoadingUIClass;

	// 跨过 World 销毁，永远记住真正的目的地
	UPROPERTY(Transient)
	FName PendingTargetMapName;

	// 手动引爆无缝加载屏！
	UFUNCTION(BlueprintCallable, Category = "LoadingScreen")
	void StartSeamlessLoadingScreen(TSoftClassPtr<class UUserWidget> CustomUI, float MinTime, FName TargetMap);

	// 手动熄灭无缝加载屏！
	UFUNCTION(BlueprintCallable, Category = "LoadingScreen")
	void StopSeamlessLoadingScreen();

	// ==============================================================================
	// 异步加载渲染管控 (Async Loading Render Control)
	// ==============================================================================
private:
	// 保留给游戏刚启动时的常规硬加载使用
	void OnPreLoadMap(const FString& MapName);

	// 唯一需要长存的变量，用于在加载期间兜住纯 Slate 灵魂免受 GC
	TSharedPtr<class SWidget> AsyncSafeWidget;
};
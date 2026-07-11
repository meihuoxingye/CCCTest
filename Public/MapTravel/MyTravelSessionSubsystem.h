// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Widgets/SWidget.h"
#include "MyTravelSessionSubsystem.generated.h"

// ==============================================================================
// 传送会话子系统 (跨越地图生死的桥梁)
// ==============================================================================
UCLASS()
class CCC_API UMyTravelSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

	// ==============================================================================
	// 生命周期与初始化
	// ==============================================================================
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ==============================================================================
	// 跨界数据总线 (Dynamic UI Transfer)
	// ==============================================================================
public:
	// 存放由发起方指定的动态 Loading UI
	UPROPERTY(Transient)
	TSoftClassPtr<class UUserWidget> PendingLoadingWidgetClass;

	// 记录目标地图名，防止过场地图错误拦截
	UPROPERTY(Transient)
	FName TargetMapName;

	// 记录传送发起的绝对时刻
	double TravelStartTime = 0.0;

	// 从传送门传过来的最短等待时间
	UPROPERTY(Transient)
	float MinimumLoadingTime = 2.0f;

	// 过场地图调用：只看不删，让 UI 数据能继续存活到新地图
	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	UClass* PeekLoadingClass();

	// 新地图调用：拿走并彻底销毁记录
	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	UClass* ConsumeLoadingClass();

	// ==============================================================================
	// 物理视口跨界护盾 (Physical Viewport Shield)
	// ==============================================================================
public:
	// 携带脱壳后的 Slate 灵魂，由 GameInstance 永久庇护
	TSharedPtr<class SWidget> CrossLevelSafeWidget;

	// 携带 UMG 原体免受 GC
	UPROPERTY()
	class UUserWidget* CrossLevelLoadingWidget = nullptr;

/*
	// ==============================================================================
	// 【硬核雷达】：全局 Slate 物理层级深度扫描
	// ==============================================================================
public:
	void StartSlateRadar();
	void StopSlateRadar();

private:
	void SlateRadarTick(float DeltaTime);
	FDelegateHandle SlateRadarHandle;
	float SlateRadarAccumulator = 0.0f;
*/
};
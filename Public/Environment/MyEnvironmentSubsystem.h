// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MyEnvironmentSubsystem.generated.h"

class UMyBiomeConfig;
class ADirectionalLight;
class AExponentialHeightFog;

/**
 * 生态环境子系统 (Environment Subsystem)
 * 专职负责大世界天气、光照、雾气等视觉表现的平滑过渡。
 * 已与核心物理传送管线彻底解耦，留待后续完善。
 */
UCLASS()
class CCC_API UMyEnvironmentSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

	// ==============================================================================
	// 环境过渡管线 (Environment Transition Pipeline)
	// ==============================================================================
public:

	// 平滑切断旧环境音效与雾气，异步过渡到目标生态的环境渲染参数
	UFUNCTION(BlueprintCallable, Category = "Environment")
	void UpdateEnvironment(UMyBiomeConfig* NewBiome, ADirectionalLight* MainLight, AExponentialHeightFog* MainFog);


	// ==============================================================================
	// 内部状态缓存 (Internal State Cache)
	// ==============================================================================
private:

	// 当前正在过渡的生态环境目标配置
	UPROPERTY()
	TObjectPtr<UMyBiomeConfig> CurrentBiomeTarget = nullptr;

	// 缓存的主光源弱指针，防范目标光源被外力物理销毁导致野指针崩溃
	TWeakObjectPtr<ADirectionalLight> CachedSunLight;

	// 缓存的大气雾弱指针
	TWeakObjectPtr<AExponentialHeightFog> CachedAtmosphereFog;

	// 驱动生态环境平滑渐变的后台异步定时器句柄
	FTimerHandle BiomeLerpTimer;

	// 生态环境渐变的当前进度 (0.0 到 1.0)
	float LerpAlpha = 0.0f;

	// 根据配置计算出的每次 Tick 的 Alpha 增量步长
	float LerpStep = 0.02f;

	// 记录生态渐变起点的太阳光旋转角度
	FRotator StartSunRotation;

	// 记录生态渐变起点的太阳光颜色
	FLinearColor StartSunColor;

	// 生态渐变定时器的 Tick 回调函数
	void ProcessBiomeLerpTick();
};
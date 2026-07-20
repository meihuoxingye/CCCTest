// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MyBiomeConfig.generated.h"

class USoundMix;

/**
 * 场景生态配置资产 (Data-Driven Biomes)
 * 用于不同区域间的平滑光影、天气与后期过渡，加入音频群落支持
 */
UCLASS()
class CCC_API UMyBiomeConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

	// ==============================================================================
	// 过渡设置 (Transition Settings)
	// ==============================================================================
public:
	// 生态环境切换的持续时间（秒）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome|Transition")
	float TransitionDuration = 2.5f;

	// ==============================================================================
	// 环境光照配置 (Environment Lighting Configuration)
	// ==============================================================================
public:
	// 太阳光角度
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome|Lighting")
	FRotator TargetSunRotation;

	// 太阳光颜色
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome|Lighting")
	FLinearColor SunLightColor;

	// ==============================================================================
	// 后期与大气 (PostProcess & Atmosphere)
	// ==============================================================================
public:
	// 雾气密度 / 大气散射数值
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome|Atmosphere")
	float FogDensity;

	// ==============================================================================
	// 音频生态 (Audio Biome)
	// ==============================================================================
public:
	// 该区域的专属声音混音
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome|Audio")
	USoundMix* BiomeSoundMix;
};
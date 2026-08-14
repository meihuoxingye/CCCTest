// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MyMapAttributeDataAsset.generated.h"

// ==============================================================================
// 地图相性枚举 (Map Phase Type)
// ==============================================================================
UENUM(BlueprintType)
enum class EMapPhaseType : uint8
{
	HasPioneeringPhase  UMETA(DisplayName = "具备开荒期 (支持夺舍原生假人)"),
	AlwaysDynamic       UMETA(DisplayName = "纯动态图 (永远强制排队捏人)")
};

/**
 * 独立的地图属性数据资产 (One Asset Per Map)
 * 彻底解耦：每个关卡拥有自己独立的数据资产文件，杜绝多人协作时的版本冲突！
 */
UCLASS(BlueprintType)
class CCC_API UMyMapAttributeDataAsset : public UDataAsset
{
	GENERATED_BODY()

	// ==============================================================================
	// 专属地图属性配置 (Map Specific Attributes)
	// ==============================================================================
public:

	// 【核心主键】：绑定对应的关卡资产 (.umap)，防 Typo 且不占常驻内存！
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Attribute|Core")
	TSoftObjectPtr<UWorld> MapAsset;

	// 属性 1：该关卡的流送与生成相性
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Attribute|Phase")
	EMapPhaseType PhaseType = EMapPhaseType::AlwaysDynamic;

	// ==============================================================================
	// 【预留扩展区】：未来该地图专属的 BGM、天气、PVP 规则等，都在此独立资产内扩展！
	// ==============================================================================
	// UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Attribute|Audio")
	// class USoundBase* DefaultBGM = nullptr;
};
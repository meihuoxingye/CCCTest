// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TeleportRoute.generated.h"

/**
 * 3A级解耦传送路由资产
 * 作为独立的实体路线文件，彻底消灭硬编码字符串 ID
 */
UCLASS(BlueprintType)
class CCC_API UTeleportRoute : public UDataAsset
{
	GENERATED_BODY()

	// ==============================================================================
	// 核心配置数据 (Core Configuration Data)
	// ==============================================================================
public:

	// 目标世界关卡的软引用：直接在编辑器下拉选择目标地图，物理杜绝拼写错误
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Teleport")
	TSoftObjectPtr<UWorld> TargetMap;
};
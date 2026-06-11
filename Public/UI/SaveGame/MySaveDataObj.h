// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SaveGame/MySaveGame.h" 
#include "MySaveDataObj.generated.h"

// 这是一个极其轻量级的纯数据壳子，用于 ListView 传递元数据
UCLASS(BlueprintType)
class CCC_API UMySaveDataObj : public UObject
{
	GENERATED_BODY()

public:
	// 携带从子系统内存镜像中取出的元数据
	UPROPERTY(BlueprintReadOnly, Category = "SaveData")
	FSaveSlotMetaData MetaData;
};
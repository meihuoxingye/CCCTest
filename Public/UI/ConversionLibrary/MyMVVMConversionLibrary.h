// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"

#include "MyMVVMConversionLibrary.generated.h"


/**
 * 
 */
UCLASS()
class CCC_API UMyMVVMConversionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	/**
	 * 全局高级 MVVM 转换器：将软引用的 Texture2D 安全转换为 SlateBrush
	 * 5.4+ 核心规范：必须显式标记 meta = (MVVMAllowed)，有且仅有一个输入参数，且必须是 BlueprintPure 纯函数
	 */
	UFUNCTION(BlueprintPure, Category = "MVVM|Conversion", meta = (MVVMAllowed))
	static struct FSlateBrush Conv_SoftTextureToBrush(TSoftObjectPtr<UTexture2D> InSoftTexture);
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MyUITools.generated.h"

/**
 * 
 */
UCLASS()
class CCC_API UMyUITools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// 供全局调用的静态函数：检测鼠标是否悬停在 UI 上
	// 玩家攻击回调函数已调用
	UFUNCTION(BlueprintCallable, Category = "UI Tools", meta = (WorldContext = "WorldContextObject"))
	static bool IsMouseOverUI(const UObject* WorldContextObject);

	/*
	 *此方法作废，SynchronizeProperties无法同步聚焦属性

	// 动态设置按钮的焦点获取能力，并强制同步到底层 Slate
	UFUNCTION(BlueprintCallable, Category = "UI Tools")
	static void SetButtonFocusable(class UButton* InButton, bool bCanFocus);
	*/
};
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonGameViewportClient.h"
#include "MyGameViewportClient.generated.h"

UCLASS()
class CCC_API UMyGameViewportClient : public UCommonGameViewportClient
{
	GENERATED_BODY()

	// ==============================================================================
	// 渲染重写与管线接口 (Render Overrides & Pipeline API)
	// ==============================================================================
		// 目前不需要任何重写，保持类的纯净，为以后真正的视口逻辑留出空间
};
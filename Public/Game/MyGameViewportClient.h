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
	// 渲染重写 (Render Overrides)
	// ==============================================================================
#pragma region
public:
	virtual void DrawTransition(class UCanvas* Canvas) override;
#pragma endregion
};
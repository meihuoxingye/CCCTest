// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/MyGameViewportClient.h"

// ==============================================================================
// 渲染重写 (Render Overrides)
// ==============================================================================
#pragma region

void UMyGameViewportClient::DrawTransition(class UCanvas* Canvas)
{
	// 彻底留空，物理超度引擎底层的三个加载点
}

#pragma endregion
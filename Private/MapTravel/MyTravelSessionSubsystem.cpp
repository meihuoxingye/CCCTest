// Fill out your copyright notice in the Description page of Project Settings.

#include "MapTravel/MyTravelSessionSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"

// ==============================================================================
// 生命周期
// ==============================================================================
#pragma region

UClass* UMyTravelSessionSubsystem::PeekLoadingClass()
{
	// 只看不删，加载并返回当前的 Loading UI 类
	if (!PendingLoadingWidgetClass.IsNull())
	{
		return PendingLoadingWidgetClass.LoadSynchronous();
	}
	return nullptr;
}

UClass* UMyTravelSessionSubsystem::ConsumeLoadingClass()
{
	// 拿走并销毁记录
	UClass* LoadedClass = nullptr;
	if (!PendingLoadingWidgetClass.IsNull())
	{
		LoadedClass = PendingLoadingWidgetClass.LoadSynchronous();
		PendingLoadingWidgetClass.Reset(); // 彻底清理
	}
	return LoadedClass;
}

#pragma endregion
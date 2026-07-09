// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/MyGameInstance.h"
#include "Widgets/SWindow.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"


// ==============================================================================
// 引擎生命周期
// ==============================================================================
#pragma region

void UMyGameInstance::Init()
{
	Super::Init();
}

void UMyGameInstance::Shutdown()
{
	HideGlobalBlackScreen();
	Super::Shutdown();
}

#pragma endregion


// ==============================================================================
// 物理层加载遮罩控制 (SWindow Overlay)
// ==============================================================================
#pragma region

void UMyGameInstance::ShowGlobalBlackScreen(TSoftClassPtr<UUserWidget> DynamicLoadingUI)
{
	// 防重入
	if (ActiveLoadingWidget.IsValid()) return;

	// 1. 动态读取发货资产，并在长生对象（GameInstance）内部实例化 UMG
	if (!DynamicLoadingUI.IsNull())
	{
		if (UClass* LoadedClass = DynamicLoadingUI.LoadSynchronous())
		{
			PersistentLoadingWidget = CreateWidget<UUserWidget>(this, LoadedClass);
			if (PersistentLoadingWidget)
			{
				PersistentLoadingWidget->AddToRoot(); // 加上全局不灭防弹衣
				ActiveLoadingWidget = PersistentLoadingWidget->TakeWidget(); // 暴力抽离 Slate 核心指针
			}
		}
	}

	// 2. 兜底策略：如果传送门没配置 UI 资产，生成绝对不漏光的纯黑色块
	if (!ActiveLoadingWidget.IsValid())
	{
		ActiveLoadingWidget = SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SColorBlock).Color(FLinearColor::Black)
			];
	}

	// 3. 【终极 3A 挂载】：将 Slate 挂载到主窗口 (OS Window)
	// 无缝传送的 RemoveAllViewportWidgets() 根本清洗不到这里，画面绝对不会闪！
	if (GEngine && GEngine->GameViewport)
	{
		TSharedPtr<SWindow> MainWindow = GEngine->GameViewport->GetWindow();
		if (MainWindow.IsValid())
		{
			MainWindow->AddOverlaySlot()
				.ZOrder(10000) // 确保在渲染层处于绝对最上方
				[
					ActiveLoadingWidget.ToSharedRef()
				];
		}
	}
}

void UMyGameInstance::HideGlobalBlackScreen()
{
	// 从主窗口撕下这块物理 UI
	if (ActiveLoadingWidget.IsValid())
	{
		if (GEngine && GEngine->GameViewport)
		{
			TSharedPtr<SWindow> MainWindow = GEngine->GameViewport->GetWindow();
			if (MainWindow.IsValid())
			{
				MainWindow->RemoveOverlaySlot(ActiveLoadingWidget.ToSharedRef());
			}
		}
		ActiveLoadingWidget.Reset();
	}

	// 安全脱下 UMG 控件的防弹衣，使其能被 GC 干净回收
	if (PersistentLoadingWidget)
	{
		PersistentLoadingWidget->RemoveFromRoot();
		PersistentLoadingWidget = nullptr;
	}
}

#pragma endregion
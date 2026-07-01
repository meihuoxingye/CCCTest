// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/MyGameInstance.h"
#include "Game/MyPreLoadScreen.h"
#include "PreLoadScreenManager.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Colors/SColorBlock.h"

// ==============================================================================
// 引擎生命周期 (Engine Lifecycle)
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
// 物理层加载遮罩控制 (Physical Loading Screen Control)
// ==============================================================================
#pragma region

void UMyGameInstance::ShowGlobalBlackScreen(TSoftClassPtr<UUserWidget> DynamicLoadingUI)
{
	if (!ActivePreLoadScreen.IsValid())
	{
		TSharedPtr<SWidget> SlateWidget = nullptr;

		// 1. 动态读取发货资产，并在长生对象（GameInstance）内部实例化 UMG
		if (!DynamicLoadingUI.IsNull())
		{
			if (UClass* LoadedClass = DynamicLoadingUI.LoadSynchronous())
			{
				PersistentLoadingWidget = CreateWidget<UUserWidget>(this, LoadedClass);
				if (PersistentLoadingWidget)
				{
					PersistentLoadingWidget->AddToRoot(); // 加上全局不灭防弹衣
					SlateWidget = PersistentLoadingWidget->TakeWidget(); // 暴力抽离 Slate 核心指针
				}
			}
		}

		// 2. 兜底策略：如果传送门没配置 UI 资产，自动切换到绝对不漏光的纯黑色块
		if (!SlateWidget.IsValid())
		{
			SlateWidget = SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SColorBlock).Color(FLinearColor::Black)
				];
		}

		// 3. 将剥离出来的 Slate 指针打包递给底层渲染层
		ActivePreLoadScreen = MakeShareable(new FMyPreLoadScreen(SlateWidget));

		if (FPreLoadScreenManager::Get())
		{
			FPreLoadScreenManager::Get()->RegisterPreLoadScreen(ActivePreLoadScreen.ToSharedRef());
		}
	}

	if (ActivePreLoadScreen.IsValid())
	{
		ActivePreLoadScreen->SetDone(false);
	}
}

void UMyGameInstance::HideGlobalBlackScreen()
{
	if (ActivePreLoadScreen.IsValid())
	{
		ActivePreLoadScreen->SetDone(true); // 让渲染层停止绘制

		if (FPreLoadScreenManager::Get())
		{
			FPreLoadScreenManager::Get()->UnRegisterPreLoadScreen(ActivePreLoadScreen.ToSharedRef());
		}
		ActivePreLoadScreen.Reset();
	}

	// 此时大世界已经流送加载完毕，安全脱下 UMG 控件的防弹衣，使其能被 GC 干净回收
	if (PersistentLoadingWidget)
	{
		PersistentLoadingWidget->RemoveFromRoot();
		PersistentLoadingWidget = nullptr;
	}
}

#pragma endregion
// Fill out your copyright notice in the Description page of Project Settings.

#include "MapTravel/MyTravelSessionSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"

// ==============================================================================
// 生命周期
// ==============================================================================
#pragma region

void UMyTravelSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UMyTravelSessionSubsystem::Deinitialize()
{
	/*StopSlateRadar();*/
	Super::Deinitialize();
}

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

/*
// ==============================================================================
// 【硬核雷达】：全局 Slate 物理层级深度扫描
// ==============================================================================
#pragma region

void UMyTravelSessionSubsystem::StartSlateRadar()
{
	if (!SlateRadarHandle.IsValid() && FSlateApplication::IsInitialized())
	{
		SlateRadarAccumulator = 0.0f;
		// 挂载到 Slate 全局 Tick，无视 World 的毁灭
		SlateRadarHandle = FSlateApplication::Get().OnPostTick().AddUObject(this, &UMyTravelSessionSubsystem::SlateRadarTick);
		UE_LOG(LogTemp, Error, TEXT("--- 物理层级深度扫描雷达已在 GameInstance [启动] ---"));
	}
}

void UMyTravelSessionSubsystem::StopSlateRadar()
{
	if (SlateRadarHandle.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().OnPostTick().Remove(SlateRadarHandle);
		SlateRadarHandle.Reset();
		UE_LOG(LogTemp, Error, TEXT("--- 物理层级深度扫描雷达已 [关闭] ---"));
	}
}

void UMyTravelSessionSubsystem::SlateRadarTick(float DeltaTime)
{
	// 限制扫描频率，每 0.5 秒扫一次
	SlateRadarAccumulator += DeltaTime;
	if (SlateRadarAccumulator < 0.5f) return;
	SlateRadarAccumulator = 0.0f;

	if (!FSlateApplication::IsInitialized()) return;
	TSharedPtr<SWindow> MainWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
	if (!MainWindow.IsValid()) return;

	TFunction<void(TSharedRef<SWidget>, int32)> Inspector;
	Inspector = [&](TSharedRef<SWidget> Current, int32 Depth)
		{
			if (Depth > 15) return; // 防死循环

			FString Indent = TEXT("");
			for (int32 i = 0; i < Depth; ++i) Indent += TEXT("  ");

			FString TypeName = Current->GetTypeAsString();

			// 只打印有效的 UI
			if (Current->GetVisibility().IsVisible() || !Current->GetVisibility().IsHitTestVisible())
			{
				UE_LOG(LogTemp, Warning, TEXT("%s[层级 %d] 类名: %s"), *Indent, Depth, *TypeName);
			}

			// 【暴力物理消除指令】
			if (TypeName.Contains(TEXT("PreLoad")) || TypeName.Contains(TEXT("Loading")) || TypeName.Contains(TEXT("Throbber")) || TypeName.Contains(TEXT("Compil")))
			{
				Current->SetVisibility(EVisibility::Collapsed);
				UE_LOG(LogTemp, Error, TEXT("！！！已精准拦截并物理消除 (Eliminate) 幽灵控件: %s ！！！"), *TypeName);
			}

			if (FChildren* Children = Current->GetChildren())
			{
				for (int32 j = 0; j < Children->Num(); ++j)
				{
					Inspector(Children->GetChildAt(j), Depth + 1);
				}
			}
		};

	UE_LOG(LogTemp, Warning, TEXT("=== [Slate 物理层级深度扫描] ==="));

	// 【已修复】：GetContent() 本身就是 TSharedRef，直接传进去即可
	Inspector(MainWindow->GetContent(), 0);
}

#pragma endregion
*/
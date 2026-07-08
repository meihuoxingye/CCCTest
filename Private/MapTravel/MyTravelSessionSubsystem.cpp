// Fill out your copyright notice in the Description page of Project Settings.

#include "MapTravel/MyTravelSessionSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Slate/SObjectWidget.h" // 【新增】：物理提取蓝图内存的钥匙

// ==============================================================================
// 生命周期
// ==============================================================================
#pragma region

void UMyTravelSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	StartSlateRadar();
}

void UMyTravelSessionSubsystem::Deinitialize()
{
	StopSlateRadar();
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

// ==============================================================================
// 【硬核雷达】：全局 Slate 物理层级深度扫描与物理溯源
// ==============================================================================
#pragma region

void UMyTravelSessionSubsystem::StartSlateRadar()
{
	if (!SlateRadarHandle.IsValid() && FSlateApplication::IsInitialized())
	{
		SlateRadarAccumulator = 0.0f;
		// 挂载到 Slate 全局 Tick，无视 World 的毁灭
		SlateRadarHandle = FSlateApplication::Get().OnPostTick().AddUObject(this, &UMyTravelSessionSubsystem::SlateRadarTick);
		UE_LOG(LogTemp, Error, TEXT("--- 物理层级深度溯源雷达已在 GameInstance [启动] ---"));
	}
}

void UMyTravelSessionSubsystem::StopSlateRadar()
{
	if (SlateRadarHandle.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().OnPostTick().Remove(SlateRadarHandle);
		SlateRadarHandle.Reset();
		UE_LOG(LogTemp, Error, TEXT("--- 物理层级深度溯源雷达已 [关闭] ---"));
	}
}

void UMyTravelSessionSubsystem::SlateRadarTick(float DeltaTime)
{
	/*
	// 限制扫描频率，每 0.5 秒扫一次
	SlateRadarAccumulator += DeltaTime;
	if (SlateRadarAccumulator < 0.5f) return;
	SlateRadarAccumulator = 0.0f;
	*/

	//---------
	// 解除频率锁，开启每帧疯狂打字模式。通过高频日志的突然中断，精确定位主线程卡死（断层开始）的时间戳。
	if (FSlateApplication::IsInitialized())
	{
		UE_LOG(LogTemp, Warning, TEXT("[UI雷达疯狂打字] --- Loading UI 正常存活中 --- 物理高精度时间戳: %f"), FPlatformTime::Seconds());
	}
	//---------

	if (!FSlateApplication::IsInitialized()) return;

	// 【致命修正】：不再只盯住主窗口！把引擎在后台偷偷生成的所有可见窗口全部抓出来！
	TArray<TSharedRef<SWindow>> AllWindows;
	FSlateApplication::Get().GetAllVisibleWindowsOrdered(AllWindows);

	TFunction<void(TSharedRef<SWidget>, int32)> Inspector;
	Inspector = [&](TSharedRef<SWidget> Current, int32 Depth)
		{
			if (Depth > 15) return; // 防死循环

			FString Indent = TEXT("");
			for (int32 i = 0; i < Depth; ++i) Indent += TEXT("  ");

			FString TypeName = Current->GetTypeAsString();

			// 【敏感词消除】
			if (TypeName.Contains(TEXT("PreLoad")) || TypeName.Contains(TEXT("Loading")) || TypeName.Contains(TEXT("Throbber")) || TypeName.Contains(TEXT("Compil")))
			{
				Current->SetVisibility(EVisibility::Collapsed);
				UE_LOG(LogTemp, Error, TEXT("%s！！！已物理消除可疑控件: %s ！！！"), *Indent, *TypeName);
			}

			if (FChildren* Children = Current->GetChildren())
			{
				for (int32 j = 0; j < Children->Num(); ++j)
				{
					Inspector(Children->GetChildAt(j), Depth + 1);
				}
			}
		};

	// 挨个强行扒开所有窗口的底裤
	for (int32 i = 0; i < AllWindows.Num(); ++i)
	{
		FString WinTitle = AllWindows[i]->GetTitle().ToString();
		/*UE_LOG(LogTemp, Warning, TEXT(">> 正在暴力扫描窗口 [%d]: 标题='%s'"), i, *WinTitle);*/
		Inspector(AllWindows[i], 0);
	}
}

#pragma endregion
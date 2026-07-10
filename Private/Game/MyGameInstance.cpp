// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/MyGameInstance.h"
#include "MoviePlayer.h"
#include "Blueprint/UserWidget.h"

// ==============================================================================
// 引擎生命周期 (Engine Lifecycle)
// ==============================================================================
#pragma region

void UMyGameInstance::Init()
{
	Super::Init();
	FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &UMyGameInstance::OnPreLoadMap);
}

void UMyGameInstance::Shutdown()
{
	FCoreUObjectDelegates::PreLoadMap.RemoveAll(this);
	Super::Shutdown();
}

#pragma endregion


// ==============================================================================
// 加载屏配置与动态接管 (Loading Screen Config & Dynamic Takeover)
// ==============================================================================
#pragma region

void UMyGameInstance::StartSeamlessLoadingScreen(TSoftClassPtr<class UUserWidget> CustomUI, float MinTime, FName TargetMap)
{
	UE_LOG(LogTemp, Error, TEXT("========== [GameInstance] StartSeamlessLoadingScreen 手动点火 =========="));
	if (IsRunningDedicatedServer()) return;

	PendingTargetMapName = TargetMap;
	UE_LOG(LogTemp, Error, TEXT("[GameInstance] 铭记真正的目标地图: %s"), *PendingTargetMapName.ToString());

	TSoftClassPtr<UUserWidget> TargetUIClass = CustomUI.IsNull() ? DefaultLoadingUIClass : CustomUI;
	UE_LOG(LogTemp, Error, TEXT("[GameInstance] 准备加载的 UI 路径: %s"), *TargetUIClass.ToString());

	if (UClass* WidgetClass = TargetUIClass.LoadSynchronous())
	{
		if (UUserWidget* LoadingWidget = CreateWidget<UUserWidget>(this, WidgetClass))
		{
			AsyncSafeWidget = LoadingWidget->TakeWidget();

			if (AsyncSafeWidget.IsValid())
			{
				UE_LOG(LogTemp, Error, TEXT("[GameInstance] 灵魂剥离成功！拿到合法的 Slate 树！"));

				if (IsMoviePlayerEnabled())
				{
					FLoadingScreenAttributes LoadingScreen;
					LoadingScreen.bAutoCompleteWhenLoadingCompletes = false; // 强制禁止自动脱落，跨越过渡地图
					LoadingScreen.bWaitForManualStop = true; // 等待抵达终点后手动拔电源

					// 【致命修复】：必须允许引擎 Tick！无缝旅行必须依赖引擎 Tick 在后台读盘，否则直接死锁！
					LoadingScreen.bAllowEngineTick = true;

					LoadingScreen.MinimumLoadingScreenDisplayTime = MinTime;
					LoadingScreen.WidgetLoadingScreen = AsyncSafeWidget;

					GetMoviePlayer()->SetupLoadingScreen(LoadingScreen);
					GetMoviePlayer()->PlayMovie(); // 强行点火接管屏幕

					UE_LOG(LogTemp, Error, TEXT("[GameInstance] PlayMovie() 执行完毕！屏幕已强制被多线程接管！"));
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("[GameInstance] 失败：IsMoviePlayerEnabled() 返回 FALSE！"));
				}
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[GameInstance] 致命失败！！！TakeWidget 返回了 NULL！"));
			}
		}
	}
	UE_LOG(LogTemp, Error, TEXT("=================================================="));
}

void UMyGameInstance::StopSeamlessLoadingScreen()
{
	UE_LOG(LogTemp, Error, TEXT("[GameInstance] StopSeamlessLoadingScreen 手动熄灭！拔掉电源！"));
	if (IsMoviePlayerEnabled())
	{
		GetMoviePlayer()->StopMovie();
	}
	AsyncSafeWidget.Reset();
	PendingTargetMapName = NAME_None;
}

#pragma endregion


// ==============================================================================
// 异步加载渲染管控 (Async Loading Render Control)
// ==============================================================================
#pragma region

void UMyGameInstance::OnPreLoadMap(const FString& MapName)
{
	UE_LOG(LogTemp, Error, TEXT("[GameInstance] OnPreLoadMap 触发(常规硬加载): %s"), *MapName);

	if (IsRunningDedicatedServer()) return;

	if (IsMoviePlayerEnabled() && GetMoviePlayer()->IsMovieCurrentlyPlaying()) return;

	if (UClass* WidgetClass = DefaultLoadingUIClass.LoadSynchronous())
	{
		if (UUserWidget* LoadingWidget = CreateWidget<UUserWidget>(this, WidgetClass))
		{
			AsyncSafeWidget = LoadingWidget->TakeWidget();
			if (IsMoviePlayerEnabled() && AsyncSafeWidget.IsValid())
			{
				FLoadingScreenAttributes LoadingScreen;
				LoadingScreen.bAutoCompleteWhenLoadingCompletes = true;
				LoadingScreen.bWaitForManualStop = false;

				// 硬加载会阻塞主线程，这里设为 false 是为了极致性能，设为 true 亦可
				LoadingScreen.bAllowEngineTick = false;

				LoadingScreen.MinimumLoadingScreenDisplayTime = 2.0f;
				LoadingScreen.WidgetLoadingScreen = AsyncSafeWidget;

				GetMoviePlayer()->SetupLoadingScreen(LoadingScreen);
			}
		}
	}
}

#pragma endregion
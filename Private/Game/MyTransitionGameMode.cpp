#include "Game/MyTransitionGameMode.h" 
#include "Kismet/GameplayStatics.h"
#include "MapTravel/MyTravelSessionSubsystem.h" // 【新增】：引入跨图会话桥梁
#include "Engine/GameInstance.h" // 【新增】：引入 GameInstance 的完整定义

// ==============================================================================
// 构造与初始化 (Construction & Initialization)
// ==============================================================================
#pragma region

AMyTransitionGameMode::AMyTransitionGameMode()
{
	// 过场世界是一个纯粹的后台流送等待区，绝对不需要 Tick 浪费算力
	PrimaryActorTick.bCanEverTick = false;

	// 【核心配置】：强制玩家以“观察者(Spectator)”身份进入！
	// 防止引擎在过场世界里傻乎乎地去生成玩家主角的 Pawn，避免模型一闪而过并节省内存
	bStartPlayersAsSpectators = true;
}

#pragma endregion

// ==============================================================================
// 生命周期逻辑 (Lifecycle Logic)
// ==============================================================================
#pragma region

void AMyTransitionGameMode::BeginPlay()
{
	Super::BeginPlay();

	UClass* TargetUIClass = nullptr;

	// 1. 【核心改造】：优先去全局桥梁中截获旧世界传过来的“定制版 UI”
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMyTravelSessionSubsystem* TravelSession = GI->GetSubsystem<UMyTravelSessionSubsystem>())
		{
			TargetUIClass = TravelSession->GetValidLoadingClassAndCleanup();
		}
	}

	// 2. 如果旧世界没有传定制 UI 过来，回退使用过场地图本地配置的 UI
	if (!TargetUIClass && LoadingWidgetClass)
	{
		TargetUIClass = LoadingWidgetClass;
	}

	// 3. 拉起最终决定的【第二棒绝对主力 UI】（有则拉起，无则纯黑屏静默流送）
	if (TargetUIClass)
	{
		APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
		if (PC && PC->IsLocalController())
		{
			ActiveLoadingWidget = CreateWidget<UUserWidget>(PC, TargetUIClass);
			if (ActiveLoadingWidget)
			{
				// 使用 9999 超高层级 (ZOrder)，确保它能盖住过场关卡里的任何 3D 杂质
				ActiveLoadingWidget->AddToViewport(9999);

				UE_LOG(LogTemp, Log, TEXT(">>> [过场世界] Loading UI 已部署，正在全速流送目标地图..."));
			}
		}
	}
}

#pragma endregion
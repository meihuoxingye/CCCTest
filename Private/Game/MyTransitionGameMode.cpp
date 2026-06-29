#include "Game/MyTransitionGameMode.h" 
#include "Kismet/GameplayStatics.h"
#include "MapTravel/MyTravelSessionSubsystem.h" 
#include "Engine/GameInstance.h" 
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

// ==============================================================================
// 构造与初始化 (Construction & Initialization)
// ==============================================================================
#pragma region

AMyTransitionGameMode::AMyTransitionGameMode()
{
	// 过场世界是一个纯粹的后台流送等待区，绝对不需要 Tick 浪费算力
	PrimaryActorTick.bCanEverTick = false;

	// 【核心配置】：强制玩家以“观察者(Spectator)”身份进入！
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

	// 1. 纯粹且唯一的数据源：从全局中继子系统中“消费”触发器传来的定制 UI
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMyTravelSessionSubsystem* TravelSession = GI->GetSubsystem<UMyTravelSessionSubsystem>())
		{
			TargetUIClass = TravelSession->ConsumeLoadingClass();
		}
	}

	// 2. 极致的参数驱动：有则拉起，无则纯黑屏静默流送，绝不自作聪明兜底
	if (TargetUIClass)
	{
		// 【核心优化】：使用遍历器替代 GetPlayerController(0)，确保只在本地客户端拉起 UI，完美支持多玩家环境
		for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			if (APlayerController* PC = Iterator->Get())
			{
				if (PC->IsLocalController())
				{
					ActiveLoadingWidget = CreateWidget<UUserWidget>(PC, TargetUIClass);
					if (ActiveLoadingWidget)
					{
						// 9999 ZOrder 确保盖住任何可能残留在 Viewport 渲染缓冲中的无效内容
						ActiveLoadingWidget->AddToViewport(9999);

						UE_LOG(LogTemp, Log, TEXT(">>> [过场世界] 成功为本地玩家 [%s] 部署动态 Loading UI, 正在全速流送目标地图..."), *PC->GetName());
					}
				}
			}
		}
	}
}

#pragma endregion
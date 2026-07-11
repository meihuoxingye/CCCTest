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
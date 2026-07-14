#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Blueprint/UserWidget.h"
#include "MyTransitionGameMode.generated.h"

// ==============================================================================
// 过场世界专属游戏模式 (Transition Map Game Mode)
// ==============================================================================
UCLASS()
class CCC_API AMyTransitionGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMyTransitionGameMode();
};
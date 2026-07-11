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

protected:
	// 内部引用：随过场世界一起灰飞烟灭，无需手动销毁
	UPROPERTY(Transient)
	UUserWidget* ActiveLoadingWidget;
};
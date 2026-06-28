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
	virtual void BeginPlay() override;

protected:
	// 在编辑器（或蓝图子类）中指定你的 Loading Widget 类 (比如 WBP_LoadingScreen)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Loading")
	TSubclassOf<UUserWidget> LoadingWidgetClass;

	// 内部引用：随过场世界一起灰飞烟灭，无需手动销毁
	UPROPERTY(Transient)
	UUserWidget* ActiveLoadingWidget;
};
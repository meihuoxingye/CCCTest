#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "MyTravelSessionSubsystem.generated.h"

// ==============================================================================
// 传送会话子系统 (跨越地图生死的桥梁)
// ==============================================================================
UCLASS()
class CCC_API UMyTravelSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

	// ==============================================================================
	// 动态 UI 数据传输 (Dynamic UI Transfer)
	// ==============================================================================
public:

	// 存放由发起方指定的动态 Loading UI（使用软引用防硬加载卡顿并支持改名重定向）
	UPROPERTY(Transient)
	TSoftClassPtr<class UUserWidget> PendingLoadingWidgetClass;

	// 获取有效的 Loading UI 类，并在获取后自动清理暂存状态，彻底消除状态残留
	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	UClass* GetValidLoadingClassAndCleanup();
};
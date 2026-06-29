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

	/** * 获取并清理挂起的 Loading UI 类
	 * 注意：此函数会“消费”掉数据，调用后 Pending 类会被置空
	 */
	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	UClass* ConsumeLoadingClass();
};
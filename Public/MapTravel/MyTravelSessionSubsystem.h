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

	// 存放由发起方指定的动态 Loading UI
	UPROPERTY(Transient)
	TSoftClassPtr<class UUserWidget> PendingLoadingWidgetClass;

	// 【新增】：记录目标地图名，防止过场地图错误拦截！
	UPROPERTY(Transient)
	FName TargetMapName;

	// 记录传送发起的绝对时刻
	double TravelStartTime = 0.0;

	// 从传送门传过来的最短等待时间
	float MinimumLoadingTime = 2.0f;

	// 过场地图调用：只看不删，让 UI 数据能继续存活到新地图
	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	UClass* PeekLoadingClass();

	// 新地图调用：拿走并彻底销毁记录
	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	UClass* ConsumeLoadingClass();

	// ==============================================================================
	// 物理视口跨界护盾 (Physical Viewport Shield)
	// ==============================================================================
public:
	// 携带脱壳后的 Slate 灵魂，由 GameInstance 永久庇护
	TSharedPtr<class SWidget> CrossLevelSafeWidget;

	// 携带 UMG 原体免受 GC
	UPROPERTY()
	class UUserWidget* CrossLevelLoadingWidget = nullptr;
};
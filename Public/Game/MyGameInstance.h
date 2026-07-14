// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
// 引入我们的转场基类
#include "UI/Transition/MyTransitionWidgetBase.h"
#include "Engine/TimerHandle.h"
#include "Engine/Engine.h" 
#include "MyGameInstance.generated.h"

// ==============================================================================
// 地图专属转场配置 (Map Transition Config)
// ==============================================================================
USTRUCT(BlueprintType)
struct FMapTransitionConfig
{
	GENERATED_BODY()

	// 离开本地图时的表现 (本世界为主)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition|Outro")
	TSoftClassPtr<class UMyTransitionWidgetBase> ScreenOffUIClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition|Outro", meta = (ClampMin = "0.1"))
	float ScreenOffDuration = 0.5f;

	// 抵达本地图时的表现 (目标世界为主)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition|Intro")
	TSoftClassPtr<class UMyTransitionWidgetBase> LoadingScreenUIClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition|Intro", meta = (ClampMin = "0.5"))
	float MinLoadingTime = 1.5f;

	// 进度条 100% 时要等待的悬停时间，完美填补公式：加载时间 = MinLoadingTime - HoldTime
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition|Intro", meta = (ClampMin = "0.0"))
	float HoldTimeAtFull = 0.5f;
};

UCLASS()
class CCC_API UMyGameInstance : public UGameInstance
{
	GENERATED_BODY()

	// ==============================================================================
	// 核心生命周期与组件 (Core Lifecycle & Components)
	// ==============================================================================
public:
	virtual void Init() override;
	virtual void Shutdown() override;

	// ==============================================================================
	// 伪加载管线 UI 管理 (Fake Loading Pipeline UI)
	// ==============================================================================
public:
	// 系统物理防死锁时间（策划只读）：
	// 当地图转场配置中的 Outro 耗时填为 0 或未配时，底层系统强制执行的 0.1 秒物理延迟。
	// 用于保证虚幻 Timer 机制的单向安全，切勿与程序硬刚此物理底线！
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Loading|Registry")
	float SystemSafeDelay = 0.1f;

	// 全局兜底加载耗时（策划只读）：
	// 当目标地图未在下方的转场字典中进行专属配置时，系统默认采用的基础加载时间参考值。
	// 用于保证常规关卡切换时，底层数据（如 World Partition）能有最基本的流送缓冲。
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Loading|Registry")
	float DefaultIntroDelay = 1.5f;

	// 全局地图转场字典！(Key: 地图名字, Value: 该地图的专属表现)
	// 策划只需要在这里统一配置一次，全宇宙的传送门自动生效！
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading|Registry")
	TMap<FName, FMapTransitionConfig> MapTransitionRegistry;

	// 默认兜底配置
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading|Registry")
	FMapTransitionConfig DefaultTransitionConfig;

	// 极速查表函数
	UFUNCTION(BlueprintPure, Category = "Loading|Registry")
	FMapTransitionConfig GetMapTransitionConfig(FName MapName) const;

	// 供子类或加载进度条提取使用的目标地图名
	UPROPERTY(Transient)
	FName PendingTargetMapName;

	// 拉起关卡设计师定制的熄屏闭合 UI
	UFUNCTION(BlueprintCallable, Category = "Loading")
	void PlayScreenOffUI(TSoftClassPtr<class UMyTransitionWidgetBase> ScreenOffUIClass, float InDuration);

	void ShowFakeLoadingScreen(TSoftClassPtr<class UMyTransitionWidgetBase> CustomUI);

	UFUNCTION(BlueprintCallable, Category = "Loading")
	void HideFakeLoadingScreen();

	// 暴露给转场基类调用的终极粉碎函数
	UFUNCTION(BlueprintCallable, Category = "Loading")
	void FinalizeLoadingScreenRemoval();

	UFUNCTION()
	void HandleStartTravel(UWorld* CurrentWorld);

	UFUNCTION()
	void HandleEndTravel(UWorld* NewWorld);

	// 向外部（如材质进度条 UI）开放的只读引擎就绪状态接口
	UFUNCTION(BlueprintCallable, Category = "Loading")
	FORCEINLINE bool IsEngineReady() const { return bEngineIsReady; }

private:
	// 完美保命符：死死抓住新世界的加载 UI，防 GC 回收
	UPROPERTY(Transient)
	UMyTransitionWidgetBase* ActiveTransitionUI;

	FTimerHandle FakeLoadingTimerHandle;

	// 自动化轮询引擎底层状态的计时器句柄
	FTimerHandle EngineReadyPollTimerHandle;

	TSharedPtr<class FBlackoutExtension, ESPMode::ThreadSafe> BlackoutExt;

	FBeginStreamingPauseDelegate BeginStreamingPauseDelegate;
	FEndStreamingPauseDelegate EndStreamingPauseDelegate;

	void OnBeginStreamingPause(FViewport* Viewport);
	void OnEndStreamingPause();

	// 底层自动化侦测函数，每隔极短时间轮询一次引擎状态
	void PollEngineReadyStatus();

	double UIStartTime = 0.0;
	bool bEngineIsReady = false;
	bool bMinTimeElapsed = false;

	// 【新增防线】：退场动画互斥锁，彻底断绝“双重退场”的可能
	bool bIsHiding = false;

	// 核心时间探针，专门记录 Persistent 进内存的绝对时间
	double PersistentLevelLoadTime = 0.0;

	void CheckAndHideLoadingScreen();
};
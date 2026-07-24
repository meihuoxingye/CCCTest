// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
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

public:

	// 离开本地图时的表现 (本世界为主)：播放“熄屏/闭眼”遮罩 UI 的软引用
	// 使用软指针防止硬加载导致的内存滚雪球
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition|Outro")
	TSoftClassPtr<class UMyScreenOffWidget> ScreenOffUIClass;

	// 离开本地图时的绝对物理等待时间，用于给屏幕完全变黑提供足够的缓冲期
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition|Outro", meta = (ClampMin = "0.1"))
	float ScreenOffDuration = 0.5f;

	// 抵达本地图时的表现 (目标世界为主)：播放“加载/睁眼”进度条 UI 的软引用
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition|Intro")
	TSoftClassPtr<class UMyLoadingScreenWidget> LoadingScreenUIClass;

	// 设计师期望的最短加载时间契约（物理防线），即使引擎瞬间加载完，UI 也必须演足这个时间
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition|Intro", meta = (ClampMin = "0.5"))
	float MinLoadingTime = 1.5f;

	// 进度条到达 100% 时要等待的悬停保留时间，用于给玩家一定的心理缓冲，完美填补平滑提速公式
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition|Intro", meta = (ClampMin = "0.0"))
	float HoldTimeAtFull = 0.5f;
};

// ==============================================================================
// 大管家：全局游戏实例 (Global Game Instance)
// ==============================================================================
UCLASS()
class CCC_API UMyGameInstance : public UGameInstance
{
	GENERATED_BODY()


	// ==============================================================================
	// 核心生命周期与组件 (Core Lifecycle & Components)
	// ==============================================================================
public:

	// 覆写原生生命周期：引擎启动、大管家实例化时调用，用于注册底层渲染钩子 and 漫游委托
	virtual void Init() override;

	// 覆写原生生命周期：游戏关闭、大管家销毁时调用，用于安全解绑委托、物理处决悬空 UI
	virtual void Shutdown() override;


	// ==============================================================================
	// 大一统传送路由管线 (Universal Routing Pipeline)
	// ==============================================================================
public:

	UPROPERTY(Transient)
	TObjectPtr<class UTeleportRoute> PendingTravelRoute;

	// 【核心新增】：必须记录黑幕 UI 指针，用于在同地图中物理抹杀它，防黑屏死锁
	UPROPERTY(Transient)
	TObjectPtr<class UMyScreenOffWidget> ActiveScreenOffUI;


	// ==============================================================================
	// 伪加载管线 UI 管理 (Fake Loading Pipeline UI)
	// ==============================================================================
public:

	// 系统物理防死锁时间（策划只读）：
	// 当 Outro 耗时填为 0 时系统强执的 0.1 秒物理延迟，保证虚幻 Timer 机制的单向安全，切勿与程序硬刚物理底线！
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Loading|Registry")
	float SystemSafeDelay = 0.1f;

	// 全局兜底加载耗时（策划只读）：
	// 目标地图未配置时采用的加载时间参考值，保证底层数据（如 World Partition）有最基本的流送缓冲。
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Loading|Registry")
	float DefaultIntroDelay = 1.5f;

	// 全局地图转场字典 (Key: 地图名字, Value: 该地图的专属表现)
	// 核心数据驱动枢纽：策划只需在此统一配置，全宇宙传送门自动抓取生效！
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading|Registry")
	TMap<FName, FMapTransitionConfig> MapTransitionRegistry;

	// 默认兜底配置：哈希查表失败时的安全退路
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading|Registry")
	FMapTransitionConfig DefaultTransitionConfig;

	// O(1) 极速哈希查表函数，纯函数无副作用，蓝图可安全高频调用
	UFUNCTION(BlueprintPure, Category = "Loading|Registry")
	FMapTransitionConfig GetMapTransitionConfig(FName MapName) const;

	// 挂起的目标地图名称缓存，供子类或加载进度条提取查表使用
	UPROPERTY(Transient)
	FName PendingTargetMapName;

	// 跨地图或同地图漫游起航前，根据配置拉起指定的熄屏闭合 UI
	UFUNCTION(BlueprintCallable, Category = "Loading")
	void PlayScreenOffPhaseUI(TSoftClassPtr<class UMyScreenOffWidget> ScreenOffUIClass, float InDuration);

	// 引擎落地新世界时调用，盲开拉起伪加载屏，并为其注入字典中的时间契约
	void PlayLoadingPhaseUI(TSoftClassPtr<class UMyLoadingScreenWidget> CustomUI);

	// 外部或内部时间契约到期时调用，发送撤退信号给活跃的加载 UI
	UFUNCTION(BlueprintCallable, Category = "Loading")
	void HideFakeLoadingScreen();

	// 暴露给转场基类调用的终极粉碎函数，仅在 UI 播完退场动画后由 UI 自己回调触发
	UFUNCTION(BlueprintCallable, Category = "Loading")
	void FinalizeLoadingScreenRemoval();

	// 无缝漫游第一阶段系统回调：切断一切旧世界留恋，拉起底层纯黑断路器
	UFUNCTION()
	void HandleStartTravel(UWorld* CurrentWorld);

	// 无缝漫游第二阶段系统回调：新世界 Persistent 落地进内存，开始流送轮询
	UFUNCTION()
	void HandleEndTravel(UWorld* NewWorld);

	// 向外部（如材质进度条 UI 内部的更新 Tick）开放的只读引擎就绪状态接口
	UFUNCTION(BlueprintCallable, Category = "Loading")
	FORCEINLINE bool IsEngineReady() const { return bEngineIsReady; }

	// 【核心新增】：同地图专属转场入口，完全复用跨地图 UI 状态机
	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void ExecuteSameMapTransition(AActor* TeleportingActor, const FTransform& TargetTransform);


private:

	// 完美保命符：通过 UPROPERTY 死死抓住新世界的加载 UI 指针，防 GC 误杀导致退场断层
	UPROPERTY(Transient)
	TObjectPtr<class UMyLoadingScreenWidget> ActiveLoadingScreenUI;

	// 驱动大管家最高主宰倒计时（最少等待时间契约）的物理定时器句柄
	FTimerHandle FakeLoadingTimerHandle;

	// 自动化轮询引擎底层流送状态的高频雷达定时器句柄
	FTimerHandle EngineReadyPollTimerHandle;

	FTimerHandle SameMapScreenOffTimerHandle;

	// 底层渲染断路器扩展，用于在渲染管线末端强制涂黑画面，彻底抹除转场初期的光追残影与闪烁
	TSharedPtr<class FBlackoutExtension, ESPMode::ThreadSafe> BlackoutExt;

	// 绑定引擎底层流送暂停挂起钩子的多线程委托实体
	FBeginStreamingPauseDelegate BeginStreamingPauseDelegate;

	// 绑定引擎底层流送恢复钩子的多线程委托实体
	FEndStreamingPauseDelegate EndStreamingPauseDelegate;

	// 占位回调函数：强行破坏并覆盖掉引擎默认拉起“三个点图标”的底层黑屏机制
	void OnBeginStreamingPause(FViewport* Viewport);

	// 占位回调函数：纯粹为了卡住多线程渲染钩子，夺回 UI 渲染控制权
	void OnEndStreamingPause();

	// 高频雷达的执行体：底层自动化侦测函数，每隔 0.05 秒轮询拷问一次引擎的真实流送状态
	void PollEngineReadyStatus();

	// 记录加载进度条正式上屏播放的绝对物理时间
	double UIStartTime = 0.0;

	// 引擎流送彻底就绪的底层物理状态标志
	bool bEngineIsReady = false;

	// 设计师设定的最短加载时间契约是否已经到期的物理标志
	bool bMinTimeElapsed = false;

	// 终极状态防线：退场动画互斥锁，彻底断绝高频信号触发“双重退场”导致的 UI 状态机踩踏
	bool bIsHiding = false;

	// 核心时间探针：专门记录 Persistent 关卡进内存的绝对物理时间戳，用于反算引擎加载耗时
	double PersistentLevelLoadTime = 0.0;

	// 内部收尾核验函数：当且仅当引擎物理就绪且时间契约到期时，安全触发 UI 动画退场
	void CheckAndHideLoadingScreen();

	// 【核心新增】：黑幕完全闭合后触发的物理折叠与 UI 换挡逻辑
	void OnSameMapScreenOffFinished(TWeakObjectPtr<AActor> TeleportingActor, FTransform TargetTransform);
};
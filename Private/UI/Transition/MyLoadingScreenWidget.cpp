// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Transition/MyLoadingScreenWidget.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Game/MyGameInstance.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h" 


// ==============================================================================
// 核心生命周期与组件 (Core Lifecycle & Components)
// ==============================================================================
#pragma region

void UMyLoadingScreenWidget::NativeConstruct()
{
	// 调用父类原生构造，确保基础 UMG 控件树及动画状态机初始化完毕
	Super::NativeConstruct();

	if (ProgressBarImage)
	{
		// 提取绑定在进度条图片组件上的材质动态实例 (MID)
		DynamicProgressMID = ProgressBarImage->GetDynamicMaterial();

		if (DynamicProgressMID)
		{
			// 将起始进度强行归零，防止复用 UI 时出现幽灵残余进度
			DynamicProgressMID->SetScalarParameterValue(TEXT("DisplayProgress"), 0.0f);
		}
	}

	// 彻底重置内部时间轴与变轨状态机的关键物理参数
	CurrentVisualPercent = 0.0f;
	ElapsedTime = 0.0f;
	bVelocityRecomputed = false;
	DynamicCalculatedSpeed = 0.0f;

	if (UWorld* World = GetWorld())
	{
		// 安全获取世界上下文，挂起驱动材质进度条的高频后台时钟，开始自主推进进度
		World->GetTimerManager().SetTimer(MaterialProgressTimerHandle, this, &UMyLoadingScreenWidget::UpdateMaterialProgressTick, UpdateInterval, true);
	}
}

void UMyLoadingScreenWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		// 物理抹杀后台材质更新时钟，防止 UI 销毁后定时器继续高频回调导致空指针崩溃
		World->GetTimerManager().ClearTimer(MaterialProgressTimerHandle);
	}

	// 移交控制权给父类完成最终的销毁管线
	Super::NativeDestruct();
}

#pragma endregion


// ==============================================================================
// 材质动态进度条管线 (Material Progress Pipeline)
// ==============================================================================
#pragma region

void UMyLoadingScreenWidget::SetLoadingTimeConfig(float InMinLoadingTime, float InHoldTime)
{
	// 接收大管家根据地图查表下发的字典契约时间
	TargetMinLoadingTime = InMinLoadingTime;
	TargetHoldTime = InHoldTime;

	// 每次接受新契约时，必须重置内部的计时与重新规划锁
	ElapsedTime = 0.0f;
	bVelocityRecomputed = false;

	// 在契约注入时，立即初始化起始物理时间，消除首帧跳变
	LastRealTime = FPlatformTime::Seconds();
}

void UMyLoadingScreenWidget::NotifyEngineReady()
{
	// 直接传递指令给基类，让基类的状态机和电池接管并执行退场（黑幕擦除）动画
	Super::NotifyEngineReady();
}

void UMyLoadingScreenWidget::UpdateMaterialProgressTick()
{
	UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance());

	// 极速防线：如果大管家已销毁或材质实例尚未就绪，直接截断本次计算
	if (!GI || !DynamicProgressMID) return;

	// 获取现实世界真实的绝对物理时间
	double CurrentRealTime = FPlatformTime::Seconds();

	// 算出真实物理耗时（防范初次运行时的巨型 Delta）
	float RealDeltaTime = (LastRealTime > 0.0) ? (float)(CurrentRealTime - LastRealTime) : 0.0f;
	LastRealTime = CurrentRealTime;

	// 累加绝对物理消逝时间
	ElapsedTime += RealDeltaTime;

	// 算出留给进度条去跑完 0%~100% 的纯净填充时间（总耗时扣除最后满条的悬停期）
	float TargetFillTime = TargetMinLoadingTime - TargetHoldTime;

	// 算出现实中还剩多少时间可以用于动画表现
	float RemainingTime = TargetFillTime - ElapsedTime;

	// 向大管家探查底层数据层流送是否真正准备就绪
	bool bIsEngineReady = GI->IsEngineReady();

	// 变轨核心：只要引擎已就绪且还没算过新速度，立刻无视一切束缚，触发重规划，彻底消除卡顿与穿帮！
	if (bIsEngineReady && !bVelocityRecomputed)
	{
		if (RemainingTime > 0.05f)
		{
			// 物理级平滑变轨公式：用剩余的未走完路程除以剩余的时间，算出一个匀速且绝对精准的冲刺速度
			DynamicCalculatedSpeed = (1.0f - CurrentVisualPercent) / RemainingTime;
		}
		else
		{
			// 剩余时间极其告急，直接启用备用的保底冲刺速度强行推满
			DynamicCalculatedSpeed = SprintSpeed;
		}

		// 扣下变轨锁，确保本次加载生命周期内只重算一次，防止高频抖动和计算浪费
		bVelocityRecomputed = true;
	}

	// 准备本帧即将应用的速度步长，默认挂载缓慢初始速度
	float CurrentSpeed = FixedInitialSpeed;

	if (bVelocityRecomputed)
	{
		// 已经成功切入变轨阶段，全面应用算出来的动态冲刺速度
		CurrentSpeed = DynamicCalculatedSpeed;
	}
	else
	{
		// 引擎仍在痛苦加载中，如果视觉进度已经触碰到防穿帮阈值，强行将速度刹车降至极微小的 2% 苟延残喘
		if (CurrentVisualPercent >= PauseThreshold)
		{
			CurrentSpeed = FixedInitialSpeed * 0.02f;
		}
	}

	/*
	*  【3A级性能防线】：开发期实时监控，发行版物理抹除！
	*/
#if !UE_BUILD_SHIPPING
	if (GEngine)
	{
		// 构建包含所有底层运作细节与状态机跳转的 Debug 格式化字符串
		FString DebugMsg = FString::Printf(TEXT("[LoadingUI] 引擎Ready: %d | 重新规划: %d | 进度: %05.1f%% | 当前速度: %05.1f%%/s | 时钟: %.2f / %.2f"),
			bIsEngineReady ? 1 : 0,
			bVelocityRecomputed ? 1 : 0,
			CurrentVisualPercent * 100.0f,
			CurrentSpeed * 100.0f,
			ElapsedTime,
			TargetFillTime);

		// 在屏幕最上层强制输出高频雷达监控面板
		GEngine->AddOnScreenDebugMessage(10086, 2.0f, FColor::Cyan, DebugMsg);
	}

	// 同步输出到后台 Output Log，方便发生 Crash 时通过日志精准定位时序
	UE_LOG(LogTemp, Warning, TEXT("[LoadingUI] 引擎Ready: %d | 重新规划: %d | 进度: %05.1f%% | 当前速度: %05.1f%%/s | 时钟: %.2f / %.2f"),
		bIsEngineReady ? 1 : 0, bVelocityRecomputed ? 1 : 0, CurrentVisualPercent * 100.0f, CurrentSpeed * 100.0f, ElapsedTime, TargetFillTime);
#endif


	// 应用本帧计算好的物理步长，推进当前的视觉百分比
	CurrentVisualPercent += RealDeltaTime * CurrentSpeed;

	if (CurrentVisualPercent >= 1.0f)
	{
		// 钳制防溢出，绝不让 UI 进度条画出超过 100% (1.0) 的荒谬边界
		CurrentVisualPercent = 1.0f;
	}

	// 将绝对安全的数值最终打入材质参数，交由 GPU 在渲染线程执行高效的纯享渲染
	DynamicProgressMID->SetScalarParameterValue(TEXT("DisplayProgress"), CurrentVisualPercent);
}

#pragma endregion
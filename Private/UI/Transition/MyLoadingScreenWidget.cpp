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
	Super::NativeConstruct();

	if (ProgressBarImage)
	{
		DynamicProgressMID = ProgressBarImage->GetDynamicMaterial();
		if (DynamicProgressMID)
		{
			DynamicProgressMID->SetScalarParameterValue(TEXT("DisplayProgress"), 0.0f);
		}
	}

	CurrentVisualPercent = 0.0f;
	ElapsedTime = 0.0f;
	bVelocityRecomputed = false;
	DynamicCalculatedSpeed = 0.0f;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(MaterialProgressTimerHandle, this, &UMyLoadingScreenWidget::UpdateMaterialProgressTick, UpdateInterval, true);
	}
}

void UMyLoadingScreenWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MaterialProgressTimerHandle);
	}

	Super::NativeDestruct();
}

#pragma endregion


// ==============================================================================
// 材质动态进度条管线 (Material Progress Pipeline)
// ==============================================================================
#pragma region

void UMyLoadingScreenWidget::SetLoadingTimeConfig(float InMinLoadingTime, float InHoldTime)
{
	TargetMinLoadingTime = InMinLoadingTime;
	TargetHoldTime = InHoldTime;
	ElapsedTime = 0.0f;
	bVelocityRecomputed = false;
}

void UMyLoadingScreenWidget::NotifyEngineReady()
{
	Super::NotifyEngineReady();
}

void UMyLoadingScreenWidget::UpdateMaterialProgressTick()
{
	UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance());
	if (!GI || !DynamicProgressMID) return;

	ElapsedTime += UpdateInterval;

	float TargetFillTime = TargetMinLoadingTime - TargetHoldTime;
	float RemainingTime = TargetFillTime - ElapsedTime;

	bool bIsEngineReady = GI->IsEngineReady();

	// 【拨乱反正】：彻底删去擅自加的 20% 限制！
	// 绝对服从你的逻辑：只要引擎就绪，且没重新规划过，立刻按剩余时间重算！
	if (bIsEngineReady && !bVelocityRecomputed)
	{
		if (RemainingTime > 0.05f)
		{
			DynamicCalculatedSpeed = (1.0f - CurrentVisualPercent) / RemainingTime;
		}
		else
		{
			DynamicCalculatedSpeed = SprintSpeed;
		}
		bVelocityRecomputed = true;
	}

	float CurrentSpeed = FixedInitialSpeed;

	if (bVelocityRecomputed)
	{
		CurrentSpeed = DynamicCalculatedSpeed;
	}
	else
	{
		if (CurrentVisualPercent >= PauseThreshold)
		{
			CurrentSpeed = FixedInitialSpeed * 0.02f;
		}
	}

	// ------------------------------------------------------------------------------
	// 实时日志监测：看清每帧的真实状态
	// ------------------------------------------------------------------------------
	if (GEngine)
	{
		FString DebugMsg = FString::Printf(TEXT("[LoadingUI] 引擎Ready: %d | 重新规划: %d | 进度: %05.1f%% | 当前速度: %05.1f%%/s | 时钟: %.2f / %.2f"),
			bIsEngineReady ? 1 : 0,
			bVelocityRecomputed ? 1 : 0,
			CurrentVisualPercent * 100.0f,
			CurrentSpeed * 100.0f,
			ElapsedTime,
			TargetFillTime);

		GEngine->AddOnScreenDebugMessage(10086, 2.0f, FColor::Cyan, DebugMsg);
	}

	UE_LOG(LogTemp, Warning, TEXT("[LoadingUI] 引擎Ready: %d | 重新规划: %d | 进度: %05.1f%% | 当前速度: %05.1f%%/s | 时钟: %.2f / %.2f"),
		bIsEngineReady ? 1 : 0, bVelocityRecomputed ? 1 : 0, CurrentVisualPercent * 100.0f, CurrentSpeed * 100.0f, ElapsedTime, TargetFillTime);
	// ------------------------------------------------------------------------------

	CurrentVisualPercent += UpdateInterval * CurrentSpeed;

	if (CurrentVisualPercent >= 1.0f)
	{
		CurrentVisualPercent = 1.0f;
	}

	DynamicProgressMID->SetScalarParameterValue(TEXT("DisplayProgress"), CurrentVisualPercent);
}

#pragma endregion
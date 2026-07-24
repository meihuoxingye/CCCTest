// Fill out your copyright notice in the Description page of Project Settings.

#include "Environment/MyEnvironmentSubsystem.h" // 请确保路径与你的实际路径匹配
#include "Environment/MyBiomeConfig.h"  // 引入你的配置数据资产
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/ExponentialHeightFogComponent.h"


// ==============================================================================
// 环境过渡管线 (Environment Transition Pipeline)
// ==============================================================================
#pragma region

void UMyEnvironmentSubsystem::UpdateEnvironment(UMyBiomeConfig* NewBiome, ADirectionalLight* MainLight, AExponentialHeightFog* MainFog)
{
	// 防御性安全拦截
	if (!NewBiome || !MainLight || !MainLight->GetComponent()) return;
	UWorld* World = GetWorld();
	if (!World) return;

	// 平滑切断旧的环境音效与 BGM 修改器
	if (CurrentBiomeTarget && CurrentBiomeTarget->BiomeSoundMix)
	{
		UGameplayStatics::PopSoundMixModifier(World, CurrentBiomeTarget->BiomeSoundMix);
	}

	// 推送新的生态混音，引擎音频系统会自动处理淡入淡出
	if (NewBiome->BiomeSoundMix)
	{
		UGameplayStatics::PushSoundMixModifier(World, NewBiome->BiomeSoundMix);
	}

	// 锁定新的生态插值目标以及相关光源引用
	CurrentBiomeTarget = NewBiome;
	CachedSunLight = MainLight;
	CachedAtmosphereFog = MainFog;

	// 重置插值进度
	LerpAlpha = 0.0f;

	// 防止配置填错导致除零崩溃，最短过渡时间限制为 0.1 秒
	float Duration = FMath::Max(0.1f, NewBiome->TransitionDuration);

	// 根据定时器的固定触发频率 (0.05s) 计算每一步的 Alpha 增量
	LerpStep = 0.05f / Duration;

	// 记录平滑插值的物理起点
	StartSunRotation = MainLight->GetComponent()->GetComponentRotation();
	StartSunColor = MainLight->GetComponent()->LightColor;

	// 挂起后台高频定时器，开始以非阻塞方式异步过渡环境渲染
	World->GetTimerManager().SetTimer(BiomeLerpTimer, this, &UMyEnvironmentSubsystem::ProcessBiomeLerpTick, 0.05f, true);
}

#pragma endregion


// ==============================================================================
// 内部状态缓存 (Internal State Cache)
// ==============================================================================
#pragma region

void UMyEnvironmentSubsystem::ProcessBiomeLerpTick()
{
	// 防御链条：世界失效、目标配置丢失、或主光源被外力物理销毁时，立即自尽停止 Tick
	if (!IsValid(GetWorld()) || !CurrentBiomeTarget || !CachedSunLight.IsValid() || !CachedSunLight->GetComponent())
	{
		if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(BiomeLerpTimer);
		return;
	}

	// 累加计算插值进度
	LerpAlpha += LerpStep;

	// 钳制范围防溢出
	LerpAlpha = FMath::Clamp(LerpAlpha, 0.0f, 1.0f);

	UDirectionalLightComponent* LightComp = CachedSunLight->GetComponent();

	// 【核心修复】：将欧拉角转换为四元数进行 Slerp (球面插值)，彻底防止太阳在跨越 0 度界限时发生后空翻！
	FRotator NewRot = FQuat::Slerp(StartSunRotation.Quaternion(), CurrentBiomeTarget->TargetSunRotation.Quaternion(), LerpAlpha).Rotator();

	FLinearColor NewColor = FMath::Lerp(StartSunColor, CurrentBiomeTarget->SunLightColor, LerpAlpha);

	// 应用状态：驱动天光旋转（时间推移）与色彩渐变
	LightComp->SetWorldRotation(NewRot);
	LightComp->SetLightColor(NewColor);

	// 同步平滑处理大气雾的浓度
	if (CachedAtmosphereFog.IsValid() && CachedAtmosphereFog->GetComponent())
	{
		CachedAtmosphereFog->GetComponent()->SetFogDensity(
			FMath::Lerp(CachedAtmosphereFog->GetComponent()->FogDensity, CurrentBiomeTarget->FogDensity, LerpAlpha)
		);
	}

	// 检查是否到达终点
	if (LerpAlpha >= 1.0f)
	{
		// 功成身退，销毁后台定时器
		if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(BiomeLerpTimer);
	}
}

#pragma endregion
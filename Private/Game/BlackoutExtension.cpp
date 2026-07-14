// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/BlackoutExtension.h"
#include "RenderGraphUtils.h"
#include "RenderTargetPool.h"
#include "SceneView.h"

// ==============================================================================
// 渲染线程断路器 (Render Thread Circuit Breaker)
// ==============================================================================
#pragma region

void FBlackoutExtension::SetupViewFamily(FSceneViewFamily& InViewFamily)
{
	if (bIsActive)
	{
		// 物理冻结 3D 场景更新：消除 5.8 硬件光追崩溃
		InViewFamily.bRealtimeUpdate = false;
	}
}

void FBlackoutExtension::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) {}
void FBlackoutExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily) {}

bool FBlackoutExtension::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	return bIsActive;
}

void FBlackoutExtension::PostRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{
	// 1. 读取线程安全变量
	// 2. 检查 RenderTarget 指针
	// 3. 【新增】：严格检查尺寸，防止 0x0 导致的 RDG 崩溃！
	if (bIsActive &&
		InViewFamily.RenderTarget &&
		InViewFamily.RenderTarget->GetSizeXY().X > 0 &&
		InViewFamily.RenderTarget->GetSizeXY().Y > 0)
	{
		// 获取当前的 RenderTarget
		FRHITexture* RenderTargetTexture = InViewFamily.RenderTarget->GetRenderTargetTexture();
		if (!RenderTargetTexture) return;

		// 注册外部纹理并添加到 RDG 清理 Pass
		FRDGTextureRef RdgRenderTarget = GraphBuilder.RegisterExternalTexture(
			CreateRenderTarget(RenderTargetTexture, TEXT("BlackoutRenderTarget"))
		);

		// 执行强制纯黑清屏，阻断残影漏光
		AddClearRenderTargetPass(GraphBuilder, RdgRenderTarget, FLinearColor::Black);
	}
}

#pragma endregion
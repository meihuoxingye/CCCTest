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
	// 拦截 3D 渲染画布直接涂黑，完美屏蔽漏光与残影。
	// 新项目落地时拉起的加载 UMG 在此阶段后独立上屏，实现无缝接力！
	if (bIsActive && InViewFamily.RenderTarget)
	{
		FRHITexture* RHITexture = InViewFamily.RenderTarget->GetRenderTargetTexture();
		if (RHITexture)
		{
			TRefCountPtr<IPooledRenderTarget> PooledRT = CreateRenderTarget(RHITexture, TEXT("BlackoutRT"));
			if (PooledRT.IsValid())
			{
				FRDGTextureRef RDGTexture = GraphBuilder.RegisterExternalTexture(PooledRT);
				AddClearRenderTargetPass(GraphBuilder, RDGTexture, FLinearColor::Black);
			}
		}
	}
}

#pragma endregion
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
		// 物理冻结 3D 场景更新：消除 5.8 硬件光追在 World Partition 卸载期间因 TLAS 重建导致的崩溃
		InViewFamily.bRealtimeUpdate = false;
	}
}

void FBlackoutExtension::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
	// 留空：不干预任何具体的 3D 视图投影计算
}

void FBlackoutExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
	// 留空：不干预渲染开始前的流程
}

bool FBlackoutExtension::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	// 快速将内部的线程安全原子变量反馈给引擎渲染管线
	return bIsActive;
}

void FBlackoutExtension::PostRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{
	// 严格检查尺寸判定防空指针：在转场、动态分辨率改变或窗口尺寸突变为 0x0 时防止向 RDG 提交非法 Pass 导致 GPU 崩溃
	if (bIsActive &&
		InViewFamily.RenderTarget &&
		InViewFamily.RenderTarget->GetSizeXY().X > 0 &&
		InViewFamily.RenderTarget->GetSizeXY().Y > 0)
	{
		// 获取当前的 RHI 纹理指针
		FRHITexture* RenderTargetTexture = InViewFamily.RenderTarget->GetRenderTargetTexture();
		if (!RenderTargetTexture) return;

		// 将原始的 RHI 纹理注册并封装为底层的 RDG 资源，以便让其安全参与 RDG 的生命周期管理
		FRDGTextureRef RdgRenderTarget = GraphBuilder.RegisterExternalTexture(
			CreateRenderTarget(RenderTargetTexture, TEXT("BlackoutRenderTarget"))
		);

		// 向渲染图提交强制纯黑清屏 Pass，截断所有 3D 画面，由于时序先于 Slate/UMG，可完美保留加载 UI 正常显示
		AddClearRenderTargetPass(GraphBuilder, RdgRenderTarget, FLinearColor::Black);
	}
}

#pragma endregion
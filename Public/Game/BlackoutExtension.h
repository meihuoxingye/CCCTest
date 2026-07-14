// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SceneViewExtension.h"
#include "HAL/ThreadSafeBool.h" // 【新增】：引入虚幻官方的线程安全类型


class CCC_API FBlackoutExtension : public FSceneViewExtensionBase
{
	// ==============================================================================
	// 渲染线程断路器 (Render Thread Circuit Breaker)
	// ==============================================================================
public:
	FBlackoutExtension(const FAutoRegister& AutoRegister) : FSceneViewExtensionBase(AutoRegister) {}

	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;

	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;

	// 严格对齐接口：操作 RenderTarget，天然放行后期 Slate/UMG UI
	virtual void PostRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override;

	// 【修改】：将普通的 bool 替换为 FThreadSafeBool，彻底消除跨线程读写冲突！
	FThreadSafeBool bIsActive = false;
};
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SceneViewExtension.h"
#include "HAL/ThreadSafeBool.h"

class CCC_API FBlackoutExtension : public FSceneViewExtensionBase
{
	// ==============================================================================
	// 渲染线程断路器 (Render Thread Circuit Breaker)
	// ==============================================================================
public:

	// 构造函数：物理注册本视图扩展到引擎的渲染管线生命周期中
	FBlackoutExtension(const FAutoRegister& AutoRegister) : FSceneViewExtensionBase(AutoRegister) {}

	// 覆写原生接口：在渲染家族初始化时触发，用于在转场期物理锁死实时 3D 渲染
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override;

	// 覆写原生接口：在单个视图初始化时调用，此处无需执行任何操作
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;

	// 覆写原生接口：在视图家族渲染开始前触发，此处留空
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;

	// 覆写原生接口：由引擎高频轮询，决定当前帧是否将本渲染扩展投入渲染管线工作
	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;

	// 覆写原生接口：在 3D 场景所有渲染 Pass 结束、UMG/Slate 界面渲染前执行，在渲染线程纯黑清屏
	virtual void PostRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override;

	// 线程安全互斥锁：将普通的 bool 替换为 FThreadSafeBool，彻底消除主线程写、渲染线程读的可见性冲突
	FThreadSafeBool bIsActive = false;
};
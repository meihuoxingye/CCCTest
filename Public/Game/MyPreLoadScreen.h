// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PreLoadScreen.h"
#include "Widgets/SWidget.h"

// ==============================================================================
// 线程级预加载屏幕 (Thread-Level PreLoad Screen with UMG Soul)
// ==============================================================================
class FMyPreLoadScreen : public IPreLoadScreen
{
public:
	FMyPreLoadScreen(TSharedPtr<SWidget> InWidget) : MyWidget(InWidget), bIsDone(false) {}
	virtual ~FMyPreLoadScreen() override {}

	// ==============================================================================
	// 强制接口实现 (Required Interface Implementation)
	// ==============================================================================
public:
	virtual void Init() override {}

	virtual void Tick(float DeltaTime) override {}

	virtual float GetAddedTickDelay() override { return 0.0f; }

	virtual bool ShouldRender() const override { return !bIsDone; }

	virtual bool IsDone() const override { return bIsDone; }

	virtual void RenderTick(FRHICommandListImmediate& RHICmdList, float DeltaTime) override {}

	virtual void OnPlay(TWeakPtr<SWindow> TargetWindow) override {}

	virtual void OnStop() override {}

	virtual void CleanUp() override {}

	virtual EPreLoadScreenTypes GetPreLoadScreenType() const override
	{
		return EPreLoadScreenTypes::EngineLoadingScreen;
	}

	virtual FName GetPreLoadScreenTag() const override
	{
		return FName(TEXT("MyPreLoadScreen"));
	}

	virtual void SetEngineLoadingFinished(bool IsEngineLoadingFinished) override {}

	virtual TSharedPtr<SWidget> GetWidget() override
	{
		return MyWidget;
	}

	virtual const TSharedPtr<const SWidget> GetWidget() const override
	{
		return MyWidget;
	}

	// ==============================================================================
	// 状态控制总线 (State Control Interface)
	// ==============================================================================
public:
	void SetDone(bool InDone) { bIsDone = InDone; }

	// ==============================================================================
	// 底层不灭资产 (Internal Protected Assets)
	// ==============================================================================
private:
	TSharedPtr<SWidget> MyWidget;
	bool bIsDone;
};
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Blueprint/UserWidget.h"
#include "MyGameInstance.generated.h"

UCLASS()
class CCC_API UMyGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;

	// 动态接收大管家平移过来的过场 UI 资产
	UFUNCTION(BlueprintCallable, Category = "LoadingScreen")
	void ShowGlobalBlackScreen(TSoftClassPtr<UUserWidget> DynamicLoadingUI = nullptr);

	UFUNCTION(BlueprintCallable, Category = "LoadingScreen")
	void HideGlobalBlackScreen();

private:
	// 穿防弹衣的 UMG 实体
	UPROPERTY()
	UUserWidget* PersistentLoadingWidget = nullptr;

	// 被抽离出来，挂在主窗口上的物理 Slate 灵魂
	TSharedPtr<class SWidget> ActiveLoadingWidget;


	// ==============================================================================
	// [雷达探针]：供外部随时查询 UI 的存活状态
	// ==============================================================================
public:

	UFUNCTION(BlueprintCallable, Category = "LoadingScreen|Debug")
	bool IsLoadingScreenActive() const
	{
		return ActiveLoadingWidget.IsValid();
	}
};
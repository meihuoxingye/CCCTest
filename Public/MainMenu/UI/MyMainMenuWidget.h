// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MyMainMenuWidget.generated.h"

class UMyCommonButtonBase;
class UMySaveMenuWidget;

/**
 * 主菜单底板 UI
 * 极简架构：不需要动画电池和堆栈拦截，直接继承 UUserWidget。
 */
UCLASS()
class CCC_API UMyMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

	// ==============================================================================
	// 生命周期与核心虚函数 (Lifecycle & Core Virtual Functions)
	// ==============================================================================
protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;


	// ==============================================================================
	// 控件绑定与配置 (Widget Binding & Config)
	// ==============================================================================
protected:
	// “开始新游戏”按钮
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UMyCommonButtonBase> Btn_StartGame;

	// “读取存档”按钮
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UMyCommonButtonBase> Btn_LoadGame;

	// “退出游戏”按钮
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UMyCommonButtonBase> Btn_QuitGame;

	// 暴露给蓝图：指定你写好的那个存档面板 UI 类 (软引用防硬加载卡顿)
	UPROPERTY(EditDefaultsOnly, Category = "MainMenu|UI")
	TSoftClassPtr<UMySaveMenuWidget> SaveMenuClass;

private:
	// 缓存生成的存档面板实例，防止玩家反复点击时重复创建引发内存泄漏
	UPROPERTY(Transient)
	TObjectPtr<UMySaveMenuWidget> SaveMenuInstance;


	// ==============================================================================
	// 交互逻辑处理 (Interaction Logic)
	// ==============================================================================
private:
	UFUNCTION()
	void OnStartGameClicked();

	UFUNCTION()
	void OnLoadGameClicked();

	UFUNCTION()
	void OnQuitGameClicked();
};
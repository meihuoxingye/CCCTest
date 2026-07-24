// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MyPlayerUIInterface.generated.h"

UINTERFACE(MinimalAPI)
class UMyPlayerUIInterface : public UInterface
{
	GENERATED_BODY()
};

// ==============================================================================
// 玩家 UI 交互契约 (Player UI Interface)
// ==============================================================================
class CCC_API IMyPlayerUIInterface
{
	GENERATED_BODY()

public:
	// 契约 1：呼出/关闭存档面板；
	// 发起方：AMySaveStationActor::Interact_Implementation()：当玩家在 3D 世界中按下交互键接触存档终端时。
	// 接收方：ATopPlayerController::ToggleSaveMenu_Implementation()：转接到 UI 处理组件的 ToggleSaveMenuWidget() 翻转 UI 面板状态；
	// 架构意义：彻底斩断 3D 物理实体与 2D 屏幕 UI 之间的强引用。存档点只需负责“开枪发送信号”，无需知道界面长什么样。
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI Interface")
	void ToggleSaveMenu();

	// 契约 2：UI 向底层发送换人请求；
	// 发起方：UMyCharacterStatusWidget::OnAvatarClicked()：当玩家点击角色头像时；
	// 接收方：ATopPlayerController::RequestCharacterSwitch_Implementation()：转接到具体处理角色附身逻辑的函数
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI Interface")
	void RequestCharacterSwitch(class ABaseCharacter* TargetCharacter);
};
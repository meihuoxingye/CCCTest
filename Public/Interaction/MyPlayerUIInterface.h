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
	// 契约 1：呼出/关闭存档面板
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI Interface")
	void ToggleSaveMenu();

	// 契约 2：UI 向底层发送换人请求
	// 发起方：UMyCharacterStatusWidget::OnAvatarClicked()：当玩家点击角色头像时
	// 接收方：ATopPlayerController::RequestCharacterSwitch_Implementation()：转接到具体处理角色附身逻辑的函数
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI Interface")
	void RequestCharacterSwitch(class ABaseCharacter* TargetCharacter);
};
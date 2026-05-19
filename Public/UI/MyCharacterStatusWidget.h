// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MyCharacterStatusWidget.generated.h"

/**
 * 
 */
UCLASS()
class CCC_API UMyCharacterStatusWidget : public UUserWidget
{
	GENERATED_BODY()


public:
	// 在 RefreshWidget 缓存当前 UI 绑定的角色指针，以便能在蓝图中获取血量等属性
	UPROPERTY(BlueprintReadOnly, Category = "Squad UI")
	TObjectPtr<class ATopCharacter> BoundCharacter;

	// 主 UI 控件里调用，传入角色指针和是否为当前选择角色，更新当前绑定角色并触发蓝图层的视觉动画逻辑
	UFUNCTION(BlueprintCallable, Category = "Squad UI")
	void RefreshWidget(class ATopCharacter* InCharacter, bool bIsSelected);

protected:
	// 留给蓝图实现的事件：当选中状态更新时，触发 UMG 缩放动画或材质参数调整
	UFUNCTION(BlueprintImplementableEvent, Category = "Squad UI")
	void OnSelectionUpdated(bool bNewIsSelected);
};

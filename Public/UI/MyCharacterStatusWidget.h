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
	// 缓存当前 UI 绑定的角色指针，供蓝图获取血量等属性
	UPROPERTY(BlueprintReadOnly, Category = "Squad UI")
	TObjectPtr<class ABaseCharacter> BoundCharacter;

	// 统一刷新接口
	UFUNCTION(BlueprintCallable, Category = "Squad UI")
	void RefreshWidget(class ABaseCharacter* InCharacter, bool bIsSelected);

protected:
	// 留给蓝图实现的事件：当选中状态更新时，触发 UMG 缩放动画或材质参数调整
	UFUNCTION(BlueprintImplementableEvent, Category = "Squad UI")
	void OnSelectionUpdated(bool bNewIsSelected);
};

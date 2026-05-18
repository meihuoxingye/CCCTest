// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MyMainHUDWidget.generated.h"

/**
 * 
 */
UCLASS()
class CCC_API UMyMainHUDWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	// 必须与 UMG 蓝图里的垂直框组件同名，用于动态容纳头像条
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UVerticalBox> SquadContainer;

	// 头像条 UI 蓝图模板类
	UPROPERTY(EditDefaultsOnly, Category = "Squad UI")
	TSubclassOf<class UMyCharacterStatusWidget> CharacterWidgetClass;

	// 核心复用池刷新算法
	UFUNCTION(BlueprintCallable, Category = "Squad UI")
	void UpdateSquadList(const TArray<class ABaseCharacter*>& Members, class ABaseCharacter* ActiveChar);

};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MyMainHUDWidget.generated.h"

/**
 * 常驻的“护目镜”层
 */
UCLASS()
class CCC_API UMyMainHUDWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	// UVerticalBox 为垂直框组件类
	// meta = (BindWidget) 是一个强力胶水标记，强制要求在主 UI 蓝图里必须摆放一个类型为垂直框、名字正好叫 SquadContainer 的组件
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UVerticalBox> SquadContainer;

	// 在蓝图细节面板暴露一个下拉菜单。允许指定动态按顺序批量生成 UI时
	// 具体要克隆哪一个 UMyCharacterStatusWidget 类型的 UI 蓝图资产
	UPROPERTY(EditDefaultsOnly, Category = "Squad UI")
	TSubclassOf<class UMyCharacterStatusWidget> CharacterWidgetClass;


	// 核心复用池刷新算法，输入存活的友方小队成员数组与正在控制的那唯一一个主角角色
	// 只读常量引用，防止拷贝消耗内存
	UFUNCTION(BlueprintCallable, Category = "Squad UI")
	void UpdateSquadList(const TArray<class ATopCharacter*>& Members, class ATopCharacter* ActiveChar);
};

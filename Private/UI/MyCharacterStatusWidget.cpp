// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/MyCharacterStatusWidget.h"

#include "Character/TopCharacter.h"


void UMyCharacterStatusWidget::RefreshWidget(ATopCharacter* InCharacter, bool bIsSelected)
{
	BoundCharacter = InCharacter;

	// 触发蓝图层的视觉动画逻辑
	// OnSelectionUpdated(bIsSelected);
}


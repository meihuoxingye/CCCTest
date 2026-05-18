// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/MyMainHUDWidget.h"

#include "Components/VerticalBox.h"
#include "UI/MyCharacterStatusWidget.h"
#include "Character/TopCharacter.h"

void UMyMainHUDWidget::UpdateSquadList(const TArray<ATopCharacter*>& Members, ATopCharacter* ActiveChar)
{
	if (!SquadContainer || !CharacterWidgetClass) return;

	int32 Required = Members.Num();
	int32 Existing = SquadContainer->GetChildrenCount();

	// 步骤一：按需复用或创建控件
	for (int32 i = 0; i < Required; ++i)
	{
		UMyCharacterStatusWidget* Target = nullptr;

		// 优先从已有控件池中获取
		if (i < Existing)
		{
			Target = Cast<UMyCharacterStatusWidget>(SquadContainer->GetChildAt(i));
		}
		// 池中控件不够，再创建新控件加入池
		else
		{
			Target = CreateWidget<UMyCharacterStatusWidget>(GetWorld(), CharacterWidgetClass);
			if (Target)
			{
				SquadContainer->AddChildToVerticalBox(Target);
			}
		}

		// 激活并刷新控件数据
		if (Target)
		{
			Target->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			Target->RefreshWidget(Members[i], Members[i] == ActiveChar);
		}
	}

	// 步骤二：对于本帧多出来的空闲控件，不进行销毁（避免GC卡顿），而是进行隐藏折叠以备后用
	for (int32 i = Required; i < Existing; ++i)
	{
		if (UWidget* Child = SquadContainer->GetChildAt(i))
		{
			Child->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}
// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/MyCharacterViewModel.h"
#include "Engine/Texture2D.h" // 解决 UTexture2D 识别问题

void UMyCharacterViewModel::SetHealth(float InValue)
{
	// 核心黑科技宏：比较新传入的值(InValue)和旧值(Health)。
	// 只有当两者“不相等”时，才会真正赋值，并底层自动触发 `FieldNotify` 广播，通知绑定的 UI 控件刷新。
	// 避免了重复赋相同值导致的无效 UI 重绘，节省性能。
	UE_MVVM_SET_PROPERTY_VALUE(Health, InValue);
}

float UMyCharacterViewModel::GetHealth() const
{
	return Health;
}

void UMyCharacterViewModel::SetMaxHealth(float InValue)
{
	// 同上，旧值新值比对，若有变化则更新并自动广播 UI 刷新
	UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, InValue);
}

float UMyCharacterViewModel::GetMaxHealth() const
{
	return MaxHealth;
}

void UMyCharacterViewModel::SetCharacterAvatar(TSoftObjectPtr<UTexture2D> InValue)
{
	// 同上，旧值新值比对，若有变化则更新并自动广播 UI 刷新
	UE_MVVM_SET_PROPERTY_VALUE(CharacterAvatar, InValue);
}

TSoftObjectPtr<UTexture2D> UMyCharacterViewModel::GetCharacterAvatar() const
{
	return CharacterAvatar;
}

void UMyCharacterViewModel::SetIsSelected(bool bInSelected)
{
	// 同上，旧值新值比对，若有变化则更新并自动广播 UI 刷新
	UE_MVVM_SET_PROPERTY_VALUE(bIsSelected, bInSelected);
}

bool UMyCharacterViewModel::IsSelected() const
{
	return bIsSelected;
}
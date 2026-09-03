// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/MyCharacterViewModel.h"
#include "Engine/Texture2D.h" // 解决 UTexture2D 识别问题

void UMyCharacterViewModel::SetHealth(float InValue)
{
	// 核心黑科技宏：比较新传入的值(InValue)和旧值(Health)。
	// 只有当两者“不相等”时，才会真正赋值，并底层自动触发 `FieldNotify` 广播，通知绑定的 UI 控件刷新。
	// 避免了重复赋相同值导致的无效 UI 重绘，节省性能。
	if (UE_MVVM_SET_PROPERTY_VALUE(Health, InValue))
	{
		// 如果主血量确实验证发生改变
		// 【连带广播】：主动替没有 Setter 的 GetHealthPercent 喊话！
		// 通知所有绑定了百分比的 UI 进度条：“底层数据变了，赶紧重新执行 GetHealthPercent 刷新画面！”
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetHealthPercent);
	}
}

float UMyCharacterViewModel::GetHealth() const
{
	return Health;
}

void UMyCharacterViewModel::SetMaxHealth(float InValue)
{
	// 同上，旧值新值比对，若有变化则更新并自动广播 UI 刷新
	if (UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, InValue))
	{
		// 【连带广播】：上限改变同理，必须手动通知 UI 重新计算百分比
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetHealthPercent);
	}
}

float UMyCharacterViewModel::GetMaxHealth() const
{
	return MaxHealth;
}

float UMyCharacterViewModel::GetHealthPercent() const
{
	// 核心防御：FMath::Max 强行把分母托底到 1.0f，绝对不可能发生除以 0 的崩溃！
	return Health / FMath::Max(MaxHealth, 1.0f);
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
// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/MyCharacterViewModel.h"
#include "Engine/Texture2D.h" // 解决 UTexture2D 识别问题

void UMyCharacterViewModel::SetHealth(float InValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(Health, InValue);
}

float UMyCharacterViewModel::GetHealth() const
{
	return Health;
}

void UMyCharacterViewModel::SetMaxHealth(float InValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, InValue);
}

float UMyCharacterViewModel::GetMaxHealth() const
{
	return MaxHealth;
}

void UMyCharacterViewModel::SetCharacterAvatar(TSoftObjectPtr<UTexture2D> InValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(CharacterAvatar, InValue);
}

TSoftObjectPtr<UTexture2D> UMyCharacterViewModel::GetCharacterAvatar() const
{
	return CharacterAvatar;
}

void UMyCharacterViewModel::SetIsSelected(bool bInSelected)
{
	UE_MVVM_SET_PROPERTY_VALUE(bIsSelected, bInSelected);
}

bool UMyCharacterViewModel::IsSelected() const
{
	return bIsSelected;
}
// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/MyCharacterViewModel.h"
#include "Engine/Texture2D.h" // 解决 UTexture2D 识别问题
#include "Styling/SlateBrush.h" // 必须包含，否则 FSlateBrush 无法按值返回

void UMyCharacterViewModel::UpdateSPMaterialData(float InSavedSP, float InRegenRate, float InLastSyncTime)
{
	FSPMaterialData NewData;
	NewData.SavedSPPercent = InSavedSP;
	NewData.RegenRatePercent = InRegenRate;
	NewData.LastSyncTime = InLastSyncTime;
	SetSPMaterialData(NewData);
}

void UMyCharacterViewModel::SetSPMaterialData(FSPMaterialData InValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(SPMaterialData, InValue);
}

FSPMaterialData UMyCharacterViewModel::GetSPMaterialData() const
{
	return SPMaterialData;
}

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
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"

#include "Styling/SlateBrush.h" // 必须引入 SlateBrush 头文件，用于 UI 绑定
// 必须包含 UTexture2D 的头文件
#include "Engine/Texture2D.h" 

#include "MyCharacterViewModel.generated.h"

/**
 * 
 */

// Blueprintable 能在新建蓝图时作为父类被继承
// BlueprintType 能在蓝图变量面板里把类型设为此类，使 UMG 资产能识别此类
// 为什么其他 C++ 类不用，因为是继承，父类有在 UCLASS() 里声明了 Blueprintable 或 BlueprintType
UCLASS(BlueprintType)
class CCC_API UMyCharacterViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()
	
public:
	// ==========================================
	// 1. 生命值属性 (Health & MaxHealth)
	// ==========================================
	UPROPERTY(BlueprintReadWrite, FieldNotify, Setter, Category = "Stats")
	float Health = 100.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Setter, Category = "Stats")
	float MaxHealth = 100.f;

	void SetHealth(float NewValue)
	{
		if (UE_MVVM_SET_PROPERTY_VALUE(Health, NewValue))
		{
			UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetHealthPercent);
		}
	}

	void SetMaxHealth(float NewValue)
	{
		if (UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, NewValue))
		{
			UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetHealthPercent);
		}
	}

	UFUNCTION(BlueprintPure, FieldNotify)
	float GetHealthPercent() const
	{
		return (MaxHealth > 0.f) ? (Health / MaxHealth) : 0.f;
	}

	// ==========================================
	// 2. 技能点属性 (SPPercent)
	// ==========================================
	UPROPERTY(BlueprintReadWrite, FieldNotify, Setter, Category = "Stats")
	float SPPercent = 0.f;

	void SetSPPercent(float NewValue)
	{
		UE_MVVM_SET_PROPERTY_VALUE(SPPercent, NewValue);
	}

	// ==========================================
	// 3. 头像属性 (CharacterAvatar & AvatarBrush)
	// ==========================================
	// 底层数据：软指针
	UPROPERTY(BlueprintReadWrite, FieldNotify, Setter, Category = "Visuals")
	TSoftObjectPtr<UTexture2D> CharacterAvatar;

	// UI 绑定专用数据：成品笔刷
	UPROPERTY(BlueprintReadWrite, FieldNotify, Setter, Category = "Visuals")
	FSlateBrush AvatarBrush;

	void SetCharacterAvatar(TSoftObjectPtr<UTexture2D> NewValue)
	{
		if (UE_MVVM_SET_PROPERTY_VALUE(CharacterAvatar, NewValue))
		{
			// 同步更新笔刷：加载软指针并包装为 Brush
			FSlateBrush NewBrush;
			if (UTexture2D* LoadedTexture = CharacterAvatar.LoadSynchronous())
			{
				NewBrush.SetResourceObject(LoadedTexture);
			}
			SetAvatarBrush(NewBrush);
		}
	}

	void SetAvatarBrush(const FSlateBrush& NewValue)
	{
		UE_MVVM_SET_PROPERTY_VALUE(AvatarBrush, NewValue);
	}

	// ==========================================
	// 4. 选中状态 (bIsSelected)
	// ==========================================
	UPROPERTY(BlueprintReadWrite, FieldNotify, Setter = SetIsSelected, Category = "State")
	bool bIsSelected = false;

	void SetIsSelected(bool NewValue)
	{
		UE_MVVM_SET_PROPERTY_VALUE(bIsSelected, NewValue);
	}
};

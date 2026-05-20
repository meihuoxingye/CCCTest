// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "UObject/SoftObjectPtr.h"
// 不能用 class 的情况： 当你告诉 UHT “请帮我生成一个访问这个属性的函数包装器”时
// 必须包含，确保 TSoftObjectPtr<UTexture2D> 不需要 class 关键字
#include "Engine/Texture2D.h" 

#include "MyCharacterViewModel.generated.h"

/**
 * 
 */

// Blueprintable 能在新建蓝图时作为父类被继承
// BlueprintType 能在蓝图变量面板里把类型设为此类，使 UMG 资产能识别此类
// 为什么其他 C++ 类不用，因为是继承，父类有在 UCLASS() 里声明了 Blueprintable 或 BlueprintType
USTRUCT(BlueprintType)
struct FSPMaterialData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Stats")
	float SavedSPPercent = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "Stats")
	float RegenRatePercent = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "Stats")
	float LastSyncTime = 0.f;

	bool operator==(const FSPMaterialData& Other) const = default;
};

UCLASS(BlueprintType)
class CCC_API UMyCharacterViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** 批量更新接口 */
	void UpdateSPMaterialData(float InSavedSP, float InRegenRate, float InLastSyncTime);

	// --- 属性定义 (5.7 核心规范：类型字符串必须与函数签名完全一致) ---

	UPROPERTY(BlueprintReadOnly, FieldNotify, Setter = SetSPMaterialData, Getter = GetSPMaterialData, Category = "MVVM")
	FSPMaterialData SPMaterialData;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Setter = SetHealth, Getter = GetHealth, Category = "MVVM")
	float Health = 100.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Setter = SetMaxHealth, Getter = GetMaxHealth, Category = "MVVM")
	float MaxHealth = 100.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Setter = SetCharacterAvatar, Getter = GetCharacterAvatar, Category = "MVVM")
	TSoftObjectPtr<UTexture2D> CharacterAvatar;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Setter = SetIsSelected, Getter = IsSelected, Category = "MVVM")
	bool bIsSelected = false;

	// --- 访问器函数 (必须标记为 UFUNCTION，且签名严丝合缝) ---

	UFUNCTION(BlueprintCallable, Category = "MVVM")
	void SetSPMaterialData(FSPMaterialData InValue);

	UFUNCTION(BlueprintPure, Category = "MVVM")
	FSPMaterialData GetSPMaterialData() const;

	UFUNCTION(BlueprintCallable, Category = "MVVM")
	void SetHealth(float InValue);

	UFUNCTION(BlueprintPure, Category = "MVVM")
	float GetHealth() const;

	UFUNCTION(BlueprintCallable, Category = "MVVM")
	void SetMaxHealth(float InValue);

	UFUNCTION(BlueprintPure, Category = "MVVM")
	float GetMaxHealth() const;

	UFUNCTION(BlueprintCallable, Category = "MVVM")
	void SetCharacterAvatar(TSoftObjectPtr<UTexture2D> InValue);

	UFUNCTION(BlueprintPure, Category = "MVVM")
	TSoftObjectPtr<UTexture2D> GetCharacterAvatar() const;

	UFUNCTION(BlueprintCallable, Category = "MVVM")
	void SetIsSelected(bool bInSelected);

	UFUNCTION(BlueprintPure, Category = "MVVM")
	bool IsSelected() const;

	// FIX 3: 专门用于绑定面板的转换函数
	UFUNCTION(BlueprintPure, Category = "MVVM|Conversion")
	static struct FSlateBrush Conv_SoftTextureToBrush(TSoftObjectPtr<UTexture2D> InSoftTexture);
};
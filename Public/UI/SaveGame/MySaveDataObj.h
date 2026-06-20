// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h" 
#include "SaveGame/MySaveGame.h" 
#include "MySaveDataObj.generated.h"

// 这是一个极其轻量级的纯数据壳子，用于 MVVM 数据总线传递元数据
UCLASS(BlueprintType)
class CCC_API UMySaveDataObj : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	// 携带从子系统内存镜像中取出的元数据
	UPROPERTY(BlueprintReadOnly, FieldNotify, Setter, Category = "SaveData")
	FSaveSlotMetaData MetaData;

	// 缓存主键，利用 FieldNotify 支持局部精准刷新
	UPROPERTY(BlueprintReadOnly, FieldNotify, Setter, Getter, Category = "SaveData")
	FString SlotName;

	// 是否为空档的布尔标示
	UPROPERTY(BlueprintReadOnly, FieldNotify, Setter = "SetIsEmptySlot", Getter = "GetIsEmptySlot", Category = "SaveData")
	bool bIsEmptySlot;

public:
	// 【核心修复 1】：利用 UHT 生成的静态常量进行 O(1) 广播，绝不走字符串查表！
	void SetMetaData(const FSaveSlotMetaData& InMetaData)
	{
		MetaData = InMetaData;
		BroadcastFieldValueChanged(UMySaveDataObj::FFieldNotificationClassDescriptor::MetaData);
	}
	FSaveSlotMetaData GetMetaData() const { return MetaData; }

	void SetSlotName(const FString& InName)
	{
		UE_MVVM_SET_PROPERTY_VALUE(SlotName, InName);
	}
	FString GetSlotName() const { return SlotName; }

	void SetIsEmptySlot(bool bIsEmpty)
	{
		UE_MVVM_SET_PROPERTY_VALUE(bIsEmptySlot, bIsEmpty);
	}
	bool GetIsEmptySlot() const { return bIsEmptySlot; }
};
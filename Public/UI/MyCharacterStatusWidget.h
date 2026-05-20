// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MyCharacterStatusWidget.generated.h"

/**
 * 
 */
UCLASS()
class CCC_API UMyCharacterStatusWidget : public UUserWidget
{
	GENERATED_BODY()


public:
	// 存储该 Widget 对应的数据源
	// 注意这里我们叫 CharacterVM，方便在 UMG 蓝图里做 Property Path 绑定
	UPROPERTY(BlueprintReadOnly, Transient, Category = "MVVM")
	TObjectPtr<class UMyCharacterViewModel> CharacterVM;

	// 替换原来的 RefreshWidget
	void SyncViewModel(class ATopCharacter* InCharacter, bool bSelected);

protected:
	// =========================================================================
	// 【正宗写法】：NativeTick 必须在这里声明！因为 UMyCharacterStatusWidget 继承自 UUserWidget
	// =========================================================================
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	// 缓存当前小卡片对应的角色 ID，供 Tick 每帧去子系统里查询
	FName CachedCharacterID;
};

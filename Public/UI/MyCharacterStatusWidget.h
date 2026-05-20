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
	/** 由外部初始化调用，同步角色引用与数据 */
	UFUNCTION(BlueprintCallable, Category = "CharacterUI")
	void SyncViewModel(class ATopCharacter* InCharacter, bool bSelected);

protected:
	virtual void NativeDestruct() override;

	/** 响应子系统 SP 数据变更的回调 */
	void OnSPDataChanged(FName CharacterID, float NewSPPercent);

	/** 内部逻辑：从子系统拉取最新数据并注入 ViewModel */
	void RefreshSPDataFromSubsystem();

private:
	/** 缓存当前绑定的角色 ID */
	UPROPERTY()
	FName CachedCharacterID;

	/** 强引用 ViewModel，确保其生命周期随 Widget */
	UPROPERTY()
	TObjectPtr<class UMyCharacterViewModel> CharacterVM;

	/**
	 * 精准卸载句柄 (2026 规范)
	 * 用于在对象销毁时从全局子系统中精准解绑，防止 RemoveAll 误伤其他逻辑
	 */
	FDelegateHandle SPChangedHandle;
};
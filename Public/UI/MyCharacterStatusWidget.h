// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

// 必须在 .h 里包含，不能只在 .cpp 里包含
#include "UI/MyCharacterViewModel.h"

#include "MyCharacterStatusWidget.generated.h"

/**
 * 角色状态 UI (MPC 终极优化版：0 Tick)
 */
UCLASS()
class CCC_API UMyCharacterStatusWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 由外部初始化调用，同步角色引用与基础静态数据 */
	UFUNCTION(BlueprintCallable, Category = "CharacterUI")
	void SyncViewModel(class ATopCharacter* InCharacter, bool bSelected);

protected:
	virtual void NativeDestruct() override;

	/** 响应子系统 SP 数据变更的回调 (事件驱动) */
	void OnSPDataChanged(FName CharacterID, float NewSPPercent);

	/** 内部逻辑：从子系统拉取最新快照并直接喂给 GPU 材质 */
	void RefreshSPDataFromSubsystem();

private:
	/** 缓存当前绑定的角色 ID，用于快速验证回调身份 */
	UPROPERTY()
	FName CachedCharacterID;

	/** 强引用 ViewModel，处理血量、头像等低频离散数据 */
	UPROPERTY()
	TObjectPtr<class UMyCharacterViewModel> CharacterVM;

	/**
	 * 精准卸载句柄
	 * 用于在对象销毁时从全局子系统中精准解绑
	 */
	FDelegateHandle SPChangedHandle;

	/** 材质动态实例，用于向 GPU 传递底层快照参数 */
	UPROPERTY()
	TObjectPtr<class UMaterialInstanceDynamic> SP_MID;

	/** 绑定 UI 里的 Image 控件 */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UImage> SPProgressBarImage;
};
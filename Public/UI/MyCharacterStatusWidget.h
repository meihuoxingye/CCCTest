// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

// 必须在 .h 里包含，不能只在 .cpp 里包含
#include "UI/MyCharacterViewModel.h"

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

	/**
	 * 关键：这是 MVVM 绑定的目标函数。
	 * 必须标记 MVVMAllowed 才能在面板中被选中。
	 * 修正点 1：参数改为 const 引用。在 MVVM 5.4+ 中，这是最标准的 Input 形式
	 * 修正点 2：确保 FSPMaterialData 的头文件已在上方 include
	 */
	UFUNCTION(BlueprintCallable, Category = "MVVM", meta = (MVVMAllowed))
	void UpdateSPMaterialParameters(const struct FSPMaterialData& InData);

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


	/** 材质动态实例，用于消除 (Eliminate) 每帧的参数寻址开销 */
	UPROPERTY()
	TObjectPtr<class UMaterialInstanceDynamic> SP_MID;

	/**
	 * 绑定 UI 里的 Image 控件
	 * 请确保 UMG 蓝图里有一个同名的 Image 控件，名称叫 SPProgressBarImage
	 */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UImage> SPProgressBarImage;
};
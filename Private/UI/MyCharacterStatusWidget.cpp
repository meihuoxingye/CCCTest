#include "UI/MyCharacterStatusWidget.h"
#include "UI/MyCharacterViewModel.h"
#include "Character/TopCharacter.h"
#include "Character/CharacterAttributeDataAsset.h"
#include "SkillSystem/SkillPointSubsystem.h"

// 引入真正存在的官方扩展组件头文件
#include "View/MVVMView.h" 
// 只要代码里有 GetWorld() 就必带此文件
#include "Engine/World.h" 
// UI 里的 Image 控件
#include "Components/Image.h"
// 用于在运行时动态修改材质参数
#include "Materials/MaterialInstanceDynamic.h"


void UMyCharacterStatusWidget::SyncViewModel(ATopCharacter* InCharacter, bool bSelected)
{
	if (!InCharacter) return;

	// 1. 初始化 ViewModel (低频业务管线)
	if (!CharacterVM)
	{
		CharacterVM = NewObject<UMyCharacterViewModel>(this);
		if (UMVVMView* ViewExtension = GetExtension<UMVVMView>())
		{
			ViewExtension->SetViewModel(FName("MyCharacterViewModel"), CharacterVM);
		}
	}
	CharacterVM->SetIsSelected(bSelected);

	// 2. 基础属性初始化
	if (const UCharacterAttributeDataAsset* Config = InCharacter->GetAttributeConfig())
	{
		CachedCharacterID = Config->CharacterID;
		CharacterVM->SetMaxHealth(Config->MaxHealth);
		CharacterVM->SetCharacterAvatar(Config->CharacterAvatar);
	}

	// 3. 【时序关键】：在建立委托绑定前，先拿到材质实例
	if (SPProgressBarImage && !SP_MID)
	{
		SP_MID = SPProgressBarImage->GetDynamicMaterial();
	}

	// 4. 重建底层系统委托监听 (抛弃 Tick，回归事件驱动)
	if (UWorld* World = GetWorld())
	{
		if (USkillPointSubsystem* SP_Sub = World->GetSubsystem<USkillPointSubsystem>())
		{
			// 防御性编程：解绑可能存在的旧句柄
			if (SPChangedHandle.IsValid())
			{
				SP_Sub->OnSPChanged.Remove(SPChangedHandle);
			}

			// 精准绑定
			SPChangedHandle = SP_Sub->OnSPChanged.AddUObject(this, &UMyCharacterStatusWidget::OnSPDataChanged);

			// UI 刚生成时，立刻主动去底层索要一次初始快照！
			RefreshSPDataFromSubsystem();
		}
	}
}

void UMyCharacterStatusWidget::OnSPDataChanged(FName CharacterID, float NewSPPercent)
{
	// 过滤广播噪音：哪怕场上有 50 个敌人同时回血、放技能，
	// 此控件只对属于自己 CachedCharacterID 的事件做出响应！
	if (CharacterID == CachedCharacterID)
	{
		RefreshSPDataFromSubsystem();
	}
}

void UMyCharacterStatusWidget::RefreshSPDataFromSubsystem()
{
	// 安全检查
	if (!SP_MID || CachedCharacterID.IsNone()) return;

	if (UWorld* World = GetWorld())
	{
		if (USkillPointSubsystem* SP_Sub = World->GetSubsystem<USkillPointSubsystem>())
		{
			// 我们不要 CPU 算好的实时百分比！
			// 我们只要底层最原始的“快照三要素”！
			if (const FCharacterSPData* DataPtr = SP_Sub->SquadSPMap.Find(CachedCharacterID))
			{
				// 防除以 0 安全锁
				const float SafeMax = FMath::Max(DataPtr->MaxSP, 1.f);

				// 【直接推给 GPU】：中间不经过任何繁琐的 MVVM 结构体打包！
				// GPU 拿到这三个参数后，结合 PlayerController 塞进 MPC 的 GlobalGameTime，自己算进度条！
				SP_MID->SetScalarParameterValue(FName("SavedSPPercent"), DataPtr->SavedSP / SafeMax);
				SP_MID->SetScalarParameterValue(FName("RegenRatePercent"), DataPtr->RegenRate / SafeMax);
				SP_MID->SetScalarParameterValue(FName("LastSyncTime"), DataPtr->LastSyncGameTime);
			}
		}
	}
}

void UMyCharacterStatusWidget::NativeDestruct()
{
	// 2026 内存安全：UI 销毁时，精准切断从子系统接过来的网线
	if (SPChangedHandle.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			if (USkillPointSubsystem* SP_Sub = World->GetSubsystem<USkillPointSubsystem>())
			{
				SP_Sub->OnSPChanged.Remove(SPChangedHandle);
			}
		}
		SPChangedHandle.Reset();
	}

	Super::NativeDestruct();
}
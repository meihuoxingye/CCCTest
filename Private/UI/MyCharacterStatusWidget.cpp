#include "UI/MyCharacterStatusWidget.h"
#include "UI/MyCharacterViewModel.h"
#include "Character/TopCharacter.h"
#include "Character/CharacterAttributeDataAsset.h"
#include "SkillSystem/SkillPointSubsystem.h"

// 引入真正存在的官方扩展组件头文件
#include "View/MVVMView.h" 
// 只要代码里有 GetWorld() 就必带此文件
#include "Engine/World.h" 


void UMyCharacterStatusWidget::SyncViewModel(ATopCharacter* InCharacter, bool bSelected)
{
	if (!InCharacter) return;

	// 1. 初始化 ViewModel (懒加载模式)
	if (!CharacterVM)
	{
		CharacterVM = NewObject<UMyCharacterViewModel>(this);

		// 在 C++ 中动态向 MVVM 扩展注入 ViewModel
		if (UMVVMView* ViewExtension = GetExtension<UMVVMView>())
		{
			// 这里的 FName 必须与 UMG 编辑器中 ViewModel 的名称完全一致
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

		// 3. 子系统委托处理
		if (UWorld* World = GetWorld())
		{
			if (USkillPointSubsystem* SP_Sub = World->GetSubsystem<USkillPointSubsystem>())
			{
				// 卸载旧句柄，防止 SyncViewModel 被多次调用导致的重复绑定
				if (SPChangedHandle.IsValid())
				{
					SP_Sub->OnSPChanged.Remove(SPChangedHandle);
				}

				// 精准绑定
				SPChangedHandle = SP_Sub->OnSPChanged.AddUObject(this, &UMyCharacterStatusWidget::OnSPDataChanged);

				// 立即同步初始状态
				RefreshSPDataFromSubsystem();
			}
		}
	}
}

void UMyCharacterStatusWidget::OnSPDataChanged(FName CharacterID, float NewSPPercent)
{
	// 过滤机制：仅当广播 ID 属于本角色时才刷新，极大降低非活动 UI 的计算开销
	if (CharacterVM && CharacterID == CachedCharacterID)
	{
		RefreshSPDataFromSubsystem();
	}
}

void UMyCharacterStatusWidget::RefreshSPDataFromSubsystem()
{
	UWorld* World = GetWorld();
	if (!World || !CharacterVM) return;

	if (USkillPointSubsystem* SP_Sub = World->GetSubsystem<USkillPointSubsystem>())
	{
		if (const FCharacterSPData* DataPtr = SP_Sub->SquadSPMap.Find(CachedCharacterID))
		{
			const float SafeMax = FMath::Max(DataPtr->MaxSP, 1.f);

			// 触发 ViewModel 更新。内部 operator== 会拦截未变动的数据，避免 Invalidation Box 频繁重绘
			CharacterVM->UpdateSPMaterialData(
				DataPtr->SavedSP / SafeMax,
				DataPtr->RegenRate / SafeMax,
				DataPtr->LastSyncGameTime
			);
		}
	}
}

void UMyCharacterStatusWidget::NativeDestruct()
{
	// 2026 核心安全实践：在 Widget 销毁时精准解绑委托
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
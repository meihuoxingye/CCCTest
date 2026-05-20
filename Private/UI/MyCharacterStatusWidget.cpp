#include "UI/MyCharacterStatusWidget.h"
#include "UI/MyCharacterViewModel.h"
#include "Character/TopCharacter.h"
#include "Character/CharacterAttributeDataAsset.h"
#include "SkillSystem/SkillPointSubsystem.h"


// 1. 引入真正存在的官方扩展组件头文件
#include "View/MVVMView.h" 

void UMyCharacterStatusWidget::SyncViewModel(ATopCharacter* InCharacter, bool bSelected)
{
	if (!InCharacter) return;

	if (!CharacterVM)
	{
		CharacterVM = NewObject<UMyCharacterViewModel>(this);

		// 获取 UMG 的 MVVM 扩展组件并设置实例
		// 注意：FName("MyCharacterViewModel") 必须和你在 UMG 里添加的模型名称完全一致
		if (UMVVMView* ViewExtension = GetExtension<UMVVMView>())
		{
			ViewExtension->SetViewModel(FName("MyCharacterViewModel"), CharacterVM);
		}
	}

	// 1. 同步选中状态
	CharacterVM->SetIsSelected(bSelected);

	// 2. 提取资产配置并同步
	if (const UCharacterAttributeDataAsset* Config = InCharacter->GetAttributeConfig())
	{
		// 把资产里的 ID 记在 UI 的小本本上，这样 Tick 才知道去查谁的蓝量！
		CachedCharacterID = Config->CharacterID;

		CharacterVM->SetMaxHealth(Config->MaxHealth);
		

		// 3. 同步头像 (底层 SetCharacterAvatar 会自动生成供 UI 绑定的 AvatarBrush)
		if (Config->CharacterType == ECharacterType::Friendly)
		{
			CharacterVM->SetCharacterAvatar(Config->CharacterAvatar);
		}

		// 4. 从子系统同步 SP 数据
		if (USkillPointSubsystem* SP_Sub = GetWorld()->GetSubsystem<USkillPointSubsystem>())
		{
			float CurrentSPPercent = SP_Sub->GetCharacterSPPercent(Config->CharacterID);
			CharacterVM->SetSPPercent(CurrentSPPercent);
		}
	}
}

void UMyCharacterStatusWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!CharacterVM || CachedCharacterID.IsNone()) return;

	if (USkillPointSubsystem* SP_Sub = GetWorld()->GetSubsystem<USkillPointSubsystem>())
	{
		// UI 每一帧主动去子系统的数学公式里抽一次最新的百分比
		float RealtimeSPPercent = SP_Sub->GetCharacterSPPercent(CachedCharacterID);

		// 拍给 ViewModel，驱动蓝条平滑上涨
		CharacterVM->SetSPPercent(RealtimeSPPercent);
	}
}

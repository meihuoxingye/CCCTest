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

#include "Components/Button.h"
#include "SwitchSubsystem/CharacterSwitchSubsystem.h"

// 引入引擎头文件以使用 GEngine 屏幕打印
#include "Engine/Engine.h"


void UMyCharacterStatusWidget::SyncViewModel(ATopCharacter* InCharacter, bool bSelected)
{
	// 防御性判空：没有角色实体就直接掐断
	if (!InCharacter) return;

	// 【新增这一行】：缓存传进来的角色指针！
	// 只有存下来了，后面点击头像（OnAvatarClicked）时才能拿它去给控制器发换人广播
	CachedCharacterRef = InCharacter;

	// ==========================================
	// 1. 初始化 ViewModel (低频业务管线对接)
	// ==========================================
	if (!CharacterVM)
	{
		// 在当前 UI 的内存名下，创建一个全新的 ViewModel 数据盒子
		CharacterVM = NewObject<UMyCharacterViewModel>(this);

		// 底层黑科技：获取虚幻自动为这个 UI 挂载的 MVVM 视图总管
		if (UMVVMView* ViewExtension = GetExtension<UMVVMView>())
		{
			// C++ 强行搭桥：把我们刚建好的数据盒子，塞进 UI 蓝图的 "MyCharacterViewModel" 插槽里
			// 等同于在 UMG 编辑器里手动配置 ViewModel
			ViewExtension->SetViewModel(FName("MyCharacterViewModel"), CharacterVM);
		}
	}
	// 利用 Setter 触发一次选中状态广播
	CharacterVM->SetIsSelected(bSelected);


	// ==========================================
	// 2. 基础属性初始化 (注入死数据)
	// ==========================================
	if (const UCharacterAttributeDataAsset* Config = InCharacter->GetAttributeConfig())
	{
		// 记下自己的身份证，后面播放广播时，靠它确定广播的是不是自己的事
		CachedCharacterID = Config->CharacterID;
		// 触发 MVVM 广播，更新血量上限和头像
		CharacterVM->SetMaxHealth(Config->MaxHealth);
		CharacterVM->SetCharacterAvatar(Config->CharacterAvatar);
	}


	// ==========================================
	// 3. 【时序关键】：在拉取第一口数据前，必须先拿到材质实例
	// ==========================================
	// !SP_MID（防重复创建）是极其关键的性能锁，实例化一个动态材质（MID）开销不算小，所以这个动作只能做一次
	if (SPProgressBarImage && !SP_MID)
	{
		// 把 Image 控件上挂着的普通的无法在运行时更改的材质，实例化为可以在运行时随时用代码调参的动态材质
		SP_MID = SPProgressBarImage->GetDynamicMaterial();
	}


	// ==========================================
	// 4. 重建底层系统委托监听 (高频 SP 管线，绝对 0 Tick)
	// ==========================================
	if (UWorld* World = GetWorld())
	{
		// 获取技能点子系统，事件驱动无 Tick，不用缓存
		if (USkillPointSubsystem* SP_Sub = World->GetSubsystem<USkillPointSubsystem>())
		{
			// 防御性编程：如果之前绑过（比如 UI 被复用），先强行拔掉老网线，防止听见两次广播
			if (SPChangedHandle.IsValid())
			{
				SP_Sub->OnSPChanged.Remove(SPChangedHandle);
			}

			// 精准绑定：把收音机调到子系统的 OnSPChanged 频道，听到广播就去执行 OnSPDataChanged
			// AddUObject 不仅执行了“绑定”这个动作，还生成了对应的专属的句柄 ID 编号，因此可以区分同一个 UI 的不同监听记录
			SPChangedHandle = SP_Sub->OnSPChanged.AddUObject(this, &UMyCharacterStatusWidget::OnSPDataChanged);

			// UI 刚生成时，立刻主动去底层索要一次初始快照，防止材质参数为空
			// 注意，SP 条的变化具体逻辑由材质内部的材质节点构成，而不是 C++ 代码
			RefreshSPDataFromSubsystem();
		}
	}
}

void UMyCharacterStatusWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (AvatarButton)
	{
		AvatarButton->OnClicked.AddDynamic(this, &UMyCharacterStatusWidget::OnAvatarClicked);
	}

	if (UWorld* World = GetWorld())
	{
		if (UCharacterSwitchSubsystem* SwitchSub = World->GetSubsystem<UCharacterSwitchSubsystem>())
		{
			ActiveCharChangedHandle = SwitchSub->OnActiveCharacterChanged.AddLambda(
				[WeakThis = TWeakObjectPtr<UMyCharacterStatusWidget>(this)](ATopCharacter* NewActiveChar)
				{
					if (UMyCharacterStatusWidget* StrongThis = WeakThis.Get())
					{
						StrongThis->HandleActiveCharacterChanged(NewActiveChar);
					}
				}
			);
		}
	}
}


void UMyCharacterStatusWidget::OnSPDataChanged(FName CharacterID, float NewSPPercent)
{
	// 过滤广播噪音：
	// 场上如果有 5 个角色，任何人扣 SP 都会触发全局广播。
	// 这里通过身份证 (CachedCharacterID) 拦截，不是自己的事绝对不管，实现 0 多余开销！
	if (CharacterID == CachedCharacterID)
	{
		RefreshSPDataFromSubsystem();
	}
}


void UMyCharacterStatusWidget::RefreshSPDataFromSubsystem()
{
	// 绝对安全锁：没材质或者连我是谁都不知道，坚决不干活
	if (!SP_MID || CachedCharacterID.IsNone()) return;

	if (UWorld* World = GetWorld())
	{
		// 获取技能点子系统，事件驱动无 Tick，不用缓存
		if (USkillPointSubsystem* SP_Sub = World->GetSubsystem<USkillPointSubsystem>())
		{
			// 获取技能点数据结构体
			// 【架构精髓】：不要 CPU 算好的实时百分比！要的是底层的原始快照！
			if (const FCharacterSPData* DataPtr = SP_Sub->SquadSPMap.Find(CachedCharacterID))
			{
				// 防御除以 0：万一配置表填了 0，强行托底为 1，防止渲染管线报 NaN(非数字) 导致黑屏
				const float SafeMax = FMath::Max(DataPtr->MaxSP, 1.f);

				// 【直接推给 GPU】：
				// 绕过繁琐的 MVVM，把 3 个核心死数据（底子、恢复率、时间戳）通过 MID 动态材质参数直插显存
				// GPU 拿到后，会自己在材质里结合 MPC 的全局时间和材质节点去疯狂推算进度条，彻底解放 CPU
				SP_MID->SetScalarParameterValue(FName("SavedSPPercent"), DataPtr->SavedSP / SafeMax);
				SP_MID->SetScalarParameterValue(FName("RegenRatePercent"), DataPtr->RegenRate / SafeMax);
				SP_MID->SetScalarParameterValue(FName("LastSyncTime"), DataPtr->LastSyncGameTime);
			}
		}
	}
}

void UMyCharacterStatusWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	if (UCharacterSwitchSubsystem* SwitchSub = GetWorld()->GetSubsystem<UCharacterSwitchSubsystem>())
	{
		SwitchSub->bIsPointerOverUI = true;
	}
}

void UMyCharacterStatusWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	if (UCharacterSwitchSubsystem* SwitchSub = GetWorld()->GetSubsystem<UCharacterSwitchSubsystem>())
	{
		SwitchSub->bIsPointerOverUI = false;
	}
}

void UMyCharacterStatusWidget::OnAvatarClicked()
{
	/*
	if (!CachedCharacterRef.IsValid()) return;

	if (UWorld* World = GetWorld())
	{
		if (UCharacterSwitchSubsystem* SwitchSub = World->GetSubsystem<UCharacterSwitchSubsystem>())
		{
			SwitchSub->OnSwitchRequest.Broadcast(CachedCharacterRef.Get());
		}
	}
	*/

	// 【测试点 1】：只要你成功点到了这个按钮，屏幕左上角就会弹出绿色文字
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Green, TEXT("【Debug】成功点到了 UI 按钮！"));
	}

	// 确保角色还活着，引用有效
	if (!CachedCharacterRef.IsValid())
	{
		// 【测试点 2】：如果点到了按钮，但角色指针是空的，弹出红色警告
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("【Error】换人失败：绑定的角色指针为空！"));
		}
		return;
	}

	// 【测试点 3】：按钮和指针都正常，准备发送换人广播，弹出青色文字并显示要换谁
	if (GEngine)
	{
		if (ATopCharacter* TargetChar = CachedCharacterRef.Get())
		{
			FString Msg = FString::Printf(TEXT("【Debug】发送换人请求，目标：%s"), *(TargetChar->GetName()));
			GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan, Msg);
		}
	}

	// 发送换人请求到总线
	if (UWorld* World = GetWorld())
	{
		if (UCharacterSwitchSubsystem* SwitchSub = World->GetSubsystem<UCharacterSwitchSubsystem>())
		{
			SwitchSub->OnSwitchRequest.Broadcast(CachedCharacterRef.Get());
		}
	}
}

void UMyCharacterStatusWidget::HandleActiveCharacterChanged(ATopCharacter* NewActiveChar)
{
	if (CharacterVM && CachedCharacterRef.IsValid())
	{
		CharacterVM->SetIsSelected(CachedCharacterRef.Get() == NewActiveChar);
	}
}


void UMyCharacterStatusWidget::NativeDestruct()
{
	// 2026 现代 C++ 内存安全：
	// UI 被销毁关掉时，它必须负责把之前在子系统那里插上的收音机网线给拔掉。
	// 否则子系统以后一发广播，就会顺着网线找到一具 UI 的尸体（野指针），游戏瞬间崩溃。
	if (SPChangedHandle.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			if (USkillPointSubsystem* SP_Sub = World->GetSubsystem<USkillPointSubsystem>())
			{
				// 拿着当年绑定时给的句柄（拔线器），精准解除监听
				SP_Sub->OnSPChanged.Remove(SPChangedHandle);
			}
		}
		// 把拔线器清空重置
		SPChangedHandle.Reset();
	}

	if (ActiveCharChangedHandle.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			if (UCharacterSwitchSubsystem* SwitchSub = World->GetSubsystem<UCharacterSwitchSubsystem>())
			{
				SwitchSub->OnActiveCharacterChanged.Remove(ActiveCharChangedHandle);
			}
		}
		ActiveCharChangedHandle.Reset();
	}

	// 必须调父类方法，走完引擎原生 UI 的销毁流程
	Super::NativeDestruct();
}
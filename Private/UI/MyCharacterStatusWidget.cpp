// Fill out your copyright notice in the Description page of Project Settings.

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
// 【必须保留！】因为 NativeOnInitialized 和 NativeDestruct 需要监听下行广播
#include "SwitchSubsystem/CharacterSwitchSubsystem.h"
// 【新增】：契约接口，UI 向上级发送请求用
#include "Interaction/MyPlayerUIInterface.h"


// ==============================================================================
// 核心生命周期与初始化 (Core Lifecycle & Initialization)
// ==============================================================================
#pragma region

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
	// 1. 虚幻底层初始化：构建基础的 Slate 控件树
	Super::NativeOnInitialized();

	// ===================== 【绑定上行请求接口 (触发源)】 =====================
	if (AvatarButton)
	{
		// 【纯 C++ 解决焦点劫持】：抢夺按钮的抢占焦点特权！
		// 确保玩家在疯狂点击头像切人时，引擎焦点始终留在 3D 世界，WASD 和空格跳跃绝不失效！
		// AvatarButton->IsFocusable = false;
		// 官方推荐，UI 蓝图静态控制，在按钮细节面板中取消勾选（Is Focusable）

		// AddDynamic 是供蓝图/UMG使用的动态多播委托绑定
		// 【架构修正】：当玩家在按钮上点击时，触发 OnAvatarClicked，通过契约接口向专属玩家控制器【盲发私密换人请求】
		AvatarButton->OnClicked.AddDynamic(this, &UMyCharacterStatusWidget::OnAvatarClicked);
	}


	// ===================== 【绑定下行监听 (内存安全版)】 =====================
	if (UWorld* World = GetWorld())
	{
		if (UCharacterSwitchSubsystem* SwitchSub = World->GetSubsystem<UCharacterSwitchSubsystem>())
		{
			// 【架构闭环】：这里就是 UI 戴上耳机，默默监听由底层的“最高法庭（子系统）”下发的全局“既定事实广播”的地方
			//
			// 【致命内存危机与防御】：
			// 为什么不直接写 AddUObject(this, ...)？
			// 因为 Subsystem（子系统）是与世界同寿的全局单例，而 UI 卡片是随时可能被销毁的临时工
			// 如果直接绑 this，UI 销毁时若忘记解绑，子系统就会捏着一个“死去的 UI 的野指针”，下次广播必定引发游戏崩溃
			//
			// 【现代 C++ 的优雅解法：弱引用捕获的 Lambda】：
			// 1. WeakThis：在 Lambda 外部，将自己(this)包装成虚幻专用的弱指针(TWeakObjectPtr)
			// 2. 捕获：将这个弱指针传进 Lambda 内部
			// 3. 强转 (StrongThis)：当子系统发广播唤醒这个 Lambda 时，先尝试把弱指针变回强指针
			// 4. 判空：如果 StrongThis 有效，说明 UI 还活着，安全执行 HandleActiveCharacterChanged 修改高亮
			//    如果 StrongThis 为空，说明 UI 已经被销毁了，if 进不去，什么都不会发生，完美化解野指针崩溃
			ActiveCharChangedHandle = SwitchSub->OnActiveCharacterChanged.AddLambda
			(
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

void UMyCharacterStatusWidget::NativeDestruct()
{
	// ===================== 【生命周期与内存安全终局 (Lifecycle & Memory Safety)】 =====================
	// 架构核心拷问：既然绑定时用了 WeakPtr (弱指针) 防崩溃，为什么还要手动解绑？
	// 答：防崩溃不等于防泄漏！
	// (省略部分注释保持原样...)

	// 【1. 清除 SP (技能点) 频道的订阅记录】
	if (SPChangedHandle.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			if (USkillPointSubsystem* SP_Sub = World->GetSubsystem<USkillPointSubsystem>())
			{
				// 拿着当年绑定时引擎颁发给你的“监听凭证 (Delegate Handle)”，精准解绑自己
				SP_Sub->OnSPChanged.Remove(SPChangedHandle);
			}
		}
		// 内存洁癖：解绑后彻底清空句柄状态，防止由于多重析构导致的重复 Remove 操作
		SPChangedHandle.Reset();
	}

	// 【2. 清除 换人宣告 (CQRS 下行事实宣告) 的订阅记录】
	if (ActiveCharChangedHandle.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			if (UCharacterSwitchSubsystem* SwitchSub = World->GetSubsystem<UCharacterSwitchSubsystem>())
			{
				// 彻底从换人广播的订阅名单中除名
				SwitchSub->OnActiveCharacterChanged.Remove(ActiveCharChangedHandle);
			}
		}
		// 清空句柄状态
		ActiveCharChangedHandle.Reset();
	}

	// ===================== 【执行底层原生销毁】 =====================
	Super::NativeDestruct();
}

#pragma endregion


// ==============================================================================
// 高频 SP 材质渲染管线 (High-Frequency SP Material Pipeline)
// ==============================================================================
#pragma region

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

#pragma endregion


// ==============================================================================
// 角色切换交互与事实监听 (Character Switch Interaction & Fact Listening)
// ==============================================================================
#pragma region

void UMyCharacterStatusWidget::OnAvatarClicked()
{
	// 1. 【内存级防御】：CachedCharacterRef 必然是一个 TWeakObjectPtr (弱指针)
	// 在发请求前，先查验这个绑定的肉体是否还在游戏世界里活着（万一刚刚被炸毁了呢）
	// 只有确保肉体有效，才允许发起切换请求，杜绝向专属玩家控制器抛出脏指令（野指针）
	if (!CachedCharacterRef.IsValid()) return;

	// 2. 【契约调用：点对点发送意图】：
	// UI 绝不走任何全局总线，它只严格遵循 IMyPlayerUIInterface 通信契约，
	// 把“自己绑定的角色指针”拍给拥有它的玩家控制器，申请灵魂交接
	if (APlayerController* PC = GetOwningPlayer())
	{
		// Implements 就是反射系统提供的一个安全查询函数，检查是否存在接口实现
		if (PC->Implements<UMyPlayerUIInterface>())
		{
			// 接口发起方，带Execute_
			// Execute_ 的作用：它会去检查 PC 的底细，如果 PC 有蓝图节点，就去触发蓝图；
			// 接收方：ATopPlayerController::RequestCharacterSwitch_Implementation()：转接到具体处理角色附身逻辑的函数
			// 如果只有 C++ 代码，就去执行 PC 的 RequestCharacterSwitch_Implementation 函数。
			// PC：接收执行方；CachedCharacterRef.Get()：传递的数据
			// .Get()：之前在头文件里，为了防内存泄漏，把 CachedCharacterRef 定义成了弱指针，需要强行转回普通指针才能用
			IMyPlayerUIInterface::Execute_RequestCharacterSwitch(PC, CachedCharacterRef.Get());
		}
	}
}

void UMyCharacterStatusWidget::HandleActiveCharacterChanged(ATopCharacter* NewActiveChar)
{
	// 1. 【下行宣告：接收既定事实】：
	// 这个函数一定是被绑在 OnActiveCharacterChanged 宣告总线上的
	// 当控制器完成了灵魂交接并全网通报后，UI 瞬间收到了这个最新上位的“新王”
	if (CharacterVM && CachedCharacterRef.IsValid())
	{
		// 2. 【MVVM 数据绑定引擎的核心】：
		// 核心逻辑判断：控制器通报的那个“新角色”，是不是【我】这张卡片绑定的角色
		// 结果是一个 bool 值 (true/false)，直接塞给 ViewModel (视图模型) 用于在视口绑定里设置
		//
		// 【解耦精髓】：这行代码绝对不会直接去写 “SetImageColor(Red)” 或 “PlayHighlightAnimation()”
		// 它只负责改写底层数据。随后，UMyCharacterViewModel 内部的数据变更广播，会自动驱动 UI 材质参数或动画
		// 实现了逻辑层与表现层的绝对分离
		CharacterVM->SetIsSelected(CachedCharacterRef.Get() == NewActiveChar);
	}
}

#pragma endregion
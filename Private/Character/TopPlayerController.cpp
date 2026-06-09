// Fill out your copyright notice in the Description page of Project Settings.
#include "Character/TopPlayerController.h"
// 增强输入
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h" 
//角色
#include "Character/TopCharacter.h"
// 换人子系统
#include "SwitchSubsystem/CharacterSwitchSubsystem.h"
// 整个虚幻引擎 Slate UI 系统的“大总管”（全局单例）
#include "Framework/Application/SlateApplication.h" 
// 时空枢纽组件
#include "Component/TimeDilationHubComponent.h"
// 引入全局 UI 射线检测工具库与子系统
#include "UI/Subsystem/MyUIManagerSubsystem.h"
// 模块化 UI 统筹组件
#include "Component/UI/MyUIHandlerComponent.h"
// 根据 IWYU 规范，显式引入缺少的引擎核心头文件，彻底消除 C2027 未定义类型编译错误
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"

ATopPlayerController::ATopPlayerController()
{
	bReplicates = false;

	// 出生时，给自己挂上时空枢纽组件背包
	TimeDilationHub = CreateDefaultSubobject<UTimeDilationHubComponent>(TEXT("TimeDilationHubComponent"));

	// 出生时，给自己挂上 UI 统筹组件背包，实现 UI 逻辑的彻底模块化解耦
	UIHandlerComp = CreateDefaultSubobject<UMyUIHandlerComponent>(TEXT("UIHandlerComponent"));
}

// ==============================================================================
// 核心生命周期与组件 (Core Lifecycle & Components)
// ==============================================================================
#pragma region

void ATopPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// 激活增强输入本地子系统
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->AddMappingContext(TopContext, 0);
	}

	// 显示鼠标
	bShowMouseCursor = true;
	// 设置鼠标样式,EMouseCursor类型的枚举
	DefaultMouseCursor = EMouseCursor::Default;

	// 三种输入模式配置结构体之一
	FInputModeGameAndUI InputModeData;

	// 配置,鼠标不会锁定在视口里
	InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	// 配置,一旦鼠标被捕获到视口里，就不会把它隐藏
	InputModeData.SetHideCursorDuringCapture(false);

	// 应用键盘鼠标输入配置
	SetInputMode(InputModeData);

	// 架构精髓：解耦。UI 蓝图不会直接获取 PlayerController 的指针，然后强行调用换人函数，而是向换人子系统的 OnSwitchRequest频道 广播
	// 而控制器找到中央子系统，告诉它：“如果有人在 OnSwitchRequest 频道发广播，请立刻调用我身上的 SwitchToSpecificCharacter 函数！”
	if (UCharacterSwitchSubsystem* SwitchSub = GetWorld()->GetSubsystem<UCharacterSwitchSubsystem>())
	{
		// 绑定 SwitchToSpecificCharacter 函数到 OnSwitchRequest 频道上
		// 虚幻底层会自动为你创建一个安全弱引用：万一这个控制器被销毁了，这根网线会自动断开，绝对不会导致野指针崩溃
		SwitchSub->OnSwitchRequest.AddUObject(this, &ATopPlayerController::SwitchToSpecificCharacter);
	}
}

#pragma endregion

// ==============================================================================
// 灵魂附身与输入绑定 (Possession & Input Setup)
// ==============================================================================
#pragma region

void ATopPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// 将普通的 InputComponent 强转为 UE5 新一代的 EnhancedInputComponent
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		// 【修改】：绑定到控制器的“战术模式”父级指令
		EnhancedInputComponent->BindAction(BulletTimeAction, ETriggerEvent::Completed, this, &ATopPlayerController::ToggleTacticalMode);

		EnhancedInputComponent->BindAction(SwitchModeAction, ETriggerEvent::Started, ToRawPtr(TimeDilationHub), &UTimeDilationHubComponent::ToggleSwitchMode);
	}
}

void ATopPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (!InPawn) return;

	// 缓存角色
	CachedMyCharacter = Cast<ATopCharacter>(InPawn);
	if (!CachedMyCharacter) return;

	// 控制器拥有了新肉体后，主动通过子系统总线向全宇宙广播
	if (UCharacterSwitchSubsystem* SwitchSub = GetWorld()->GetSubsystem<UCharacterSwitchSubsystem>())
	{
		SwitchSub->OnActiveCharacterChanged.Broadcast(CachedMyCharacter);
	}

	// 当玩家控制权交接时，通过组件立即重刷 UI（可选保障，通常 GameMode 也会在后续广播）
	if (UIHandlerComp)
	{
		UIHandlerComp->UpdateHUD();
	}
}

void ATopPlayerController::OnUnPossess()
{
	Super::OnUnPossess();

	CachedMyCharacter = nullptr;
}

#pragma endregion

// ==============================================================================
// 战术指令与总线转发 (Tactical Commands & Bus Forwarding)
// ==============================================================================
#pragma region

bool ATopPlayerController::ProcessGlobalClick()
{
	/**	 * 如果左键回调函数很多，就不能用这种 if 形式，而要考虑“状态机”或“责任链”	 */

	 // 1. 第一道防线（UI 子集层）：优先派发给 UI 系统处理点击、清扫内存与关闭面板
	 // 尝试获取当前控制器所属的本地玩家 (LocalPlayer)
	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		// 获取挂载在该本地玩家身上的自定义 UI 管理器子系统
		if (UMyUIManagerSubsystem* UIMgr = LP->GetSubsystem<UMyUIManagerSubsystem>())
		{
			// 如果 UI 子系统说：“我拦截了点击（因为关了弹窗，或者点在了图片上）”，直接 return true
			if (UIMgr->ProcessUIClick())
			{
				// 返回 true 告诉引擎底层：本次鼠标点击已被 UI 层消耗完毕，绝不能往下传给 3D 世界触发开枪
				return true;
			}
		}
	}

	// 2. 第二道防线（全局状态层）：判定本次点击是否被特殊游戏逻辑（如切人模式）拦截
	// 检查当前是否处于“切人子弹时间”状态
	if (IsSwitchModeActive())
	{
		// 如果在切人模式下点了左键（且没点在UI上），这代表玩家想“取消切人”，所以强制关闭切人状态
		SetSwitchMode(false);
		// 拦截并没收本次点击，防止取消切人的这一下左键操作导致角色走火开枪
		return true;
	}

	// 如果既没有点在 UI 上，也没有处于切人状态，则放行本次点击（返回 false），允许角色正常开火射击
	return false;
}

bool ATopPlayerController::IsSwitchModeActive() const
{
	// 三元运算符防崩：如果时间枢纽组件存在，返回它的状态；如果组件竟然被意外销毁了，强制返回 false 托底
	return TimeDilationHub ? TimeDilationHub->bIsSwitchModeActive : false;
}

void ATopPlayerController::SetSwitchMode(bool bEnable)
{
	// 安全校验与指令下发：控制器不负责具体的时间流速计算
	// 确保时空枢纽组件存在后，将“开启/关闭切人模式”的实际逻辑完全托付给该组件执行
	if (TimeDilationHub) TimeDilationHub->SetSwitchMode(bEnable);
}

void ATopPlayerController::ToggleTacticalMode()
{
	// 向两个不同职责的子集下发专属于它们的执行指令，层级极其清晰
	if (TimeDilationHub) TimeDilationHub->ToggleBulletTime();
	if (UIHandlerComp) UIHandlerComp->ToggleTacticalWidget();
}

// 换人系统总线的最终执行者：接收 UI 传来的“目标角色”并执行灵魂交接
// 核心职责：处理角色附身逻辑，并安全清理残留的 UI 状态与输入焦点
void ATopPlayerController::SwitchToSpecificCharacter(ATopCharacter* TargetCharacter)
{
	// 防御验证：目标为空，或者点的正是自己
	if (!TargetCharacter || TargetCharacter == CachedMyCharacter)
	{
		// 如果是因为点击了无效目标导致换人失败，依然要确保退出切人的子弹时间
		if (IsSwitchModeActive()) SetSwitchMode(false);

		// 只要鼠标点击过 UI，UI 就会霸占引擎的“输入焦点”
		// 这会导致你下一次按 Tab 键、按 WASD 键，全部被 UI 吞掉，游戏毫无反应
		// 这句话就是“上帝之手”，强行把焦点从 UI 剥夺，摔回给 3D 游戏画面
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
		return;
	}

	// 移交控制权：调用虚幻引擎底层的灵魂附身
	// 此操作会自动依次触发旧角色的 OnUnPossess 和新角色的 OnPossess
	Possess(TargetCharacter);

	// 换人成功后，自动退出切人模式（恢复正常时间流逝）
	if (IsSwitchModeActive()) SetSwitchMode(false);

	// 同上：确保附身新躯体后，玩家能立刻用键盘鼠标操控角色，而不是按键继续被 UI 吞噬
	FSlateApplication::Get().SetAllUserFocusToGameViewport();
}

#pragma endregion
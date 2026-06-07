#include "Character/TopPlayerController.h"
// 增强输入
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h" 
//角色
#include "Character/TopCharacter.h"
// 引入 UI 及 GameMode 头文件
#include "UI/MyMainHUDWidget.h"
#include "Game/MyGameModeBase.h"
// 换人子系统
#include "SwitchSubsystem/CharacterSwitchSubsystem.h"
// 整个虚幻引擎 Slate UI 系统的“大总管”（全局单例）
#include "Framework/Application/SlateApplication.h" 
// 时空枢纽组件
#include "Component/TimeDilationHubComponent.h"
// 【新增】：严格按照 IWYU 路径引入战术面板基类
#include "UI/ActivatableWidget/MyActivatableWidgetBase.h"

// 【新增】：引入全局 UI 射线检测工具库
#include "Tools/MyUITools.h"

#include "UI/Subsystem/MyUIManagerSubsystem.h"

ATopPlayerController::ATopPlayerController()
{
	bReplicates = false;

	// 出生时，给自己挂上时空枢纽组件背包
	TimeDilationHub = CreateDefaultSubobject<UTimeDilationHubComponent>(TEXT("TimeDilationHubComponent"));
}

// ==============================================================================
// 核心生命周期与组件 (Core Lifecycle & Components)
// ==============================================================================
#pragma region
void ATopPlayerController::BeginPlay()
{
	Super::BeginPlay();

#pragma region 输入与鼠标
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
#pragma endregion

	// HUD 控件的初始化与首帧构建
	if (MainHUDClass)
	{
		// 创造主 UI 组件，拥有者为当前玩家控制器
		MainHUDInstance = CreateWidget<UMyMainHUDWidget>(this, MainHUDClass);

		if (MainHUDInstance)
		{
			// 将主 UI 组件添加到视口
			MainHUDInstance->AddToViewport();
			// 更新 UI 界面
			UpdateHUD();
		}
	}

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
	UEnhancedInputComponent* EnhancedInputComponent =
		CastChecked<UEnhancedInputComponent>(InputComponent);

	// 绑定回调
	if (EnhancedInputComponent)
	{
		EnhancedInputComponent->BindAction(BulletTimeAction, ETriggerEvent::Completed, this, &ATopPlayerController::ToggleTacticalWidget);
		EnhancedInputComponent->BindAction(SwitchModeAction, ETriggerEvent::Started, TimeDilationHub.Get(), &UTimeDilationHubComponent::ToggleSwitchMode);
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

	// 当玩家控制权交接时，立即重刷 UI
	UpdateHUD();
}

void ATopPlayerController::OnUnPossess()
{
	Super::OnUnPossess();

	CachedMyCharacter = nullptr;
}

void ATopPlayerController::ToggleTacticalWidget()
{
	// 如果时间膨胀组件存在，触发它内部的子弹时间切换逻辑
	if (TimeDilationHub) TimeDilationHub->ToggleBulletTime();

	// 懒加载模式：如果战术 UI 实例还未创建，且蓝图里配置了具体的类模板，则开始创建
	if (!TacticalWidgetInstance && TacticalWidgetClass)
	{
		// 在当前控制器的内存名下创建这个战术 UI 蓝图的实例
		TacticalWidgetInstance = CreateWidget<UMyActivatableWidgetBase>(this, TacticalWidgetClass);

		// 确保 UI 实例创建成功
		if (TacticalWidgetInstance)
		{
			// 添加到屏幕视口，ZOrder 设置为 100 保证它盖在游戏画面和其他普通 UI 之上
			// ZOrder（Z 轴排序/层级），数字越大，这个 UI 所在的层就越靠前，会遮挡住底下的 UI
			TacticalWidgetInstance->AddToViewport(100);
			// 初始创建时强制设为隐藏（Collapsed），防止在还没播放动画时出现一帧的画面闪烁
			TacticalWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);
			// 动态绑定 UI 内部抛出的“关闭请求”委托，这样当 UI 自己想要关闭时，就会回调当前这个函数执行统一的关闭流程
			TacticalWidgetInstance->OnCloseRequested.AddUniqueDynamic(this, &ATopPlayerController::ToggleTacticalWidget);
		}
	}

	// 安全校验：如果 UI 实例依然为空（比如粗心没配置类模板），直接退出防止引发野指针崩溃
	if (!TacticalWidgetInstance) return;

	// 获取增强输入系统的本地玩家子系统，用于后续插拔输入映射上下文 (IMC)
	auto* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());

	// 核心状态判断：通过检查“是否已经挂载了战术专属 IMC”来决定现在是要“打开面板”还是“关闭面板”
	// 高级做法：利用输入子系统与输入映射上下文判断此时 UI 应打开还是关闭，IMC 为底层逻辑，而 UI 状态则为表象
	// 子系统存在，则可判断上面的 IMC 是否已经挂载。未挂载则说明面板不存在要开启，反之则是要关闭
	// 子系统不存在，可能是刚运行游戏，直接默认要开启
	bool bIsActivating = Subsystem ? !Subsystem->HasMappingContext(TacticalIMC) : true;

	// 如果判定为：准备打开战术面板
	if (bIsActivating)
	{
		// 呼叫 UI 实例执行它自己的“被激活”逻辑（比如重置进度、播放展开动画、主动入栈防穿透）
		TacticalWidgetInstance->OnWidgetActivated();

		// 为增强输入系统挂载战术面板专用的 IMC，优先级设为 10（高于默认的 0）
		// IMC（输入映射上下文）是可以像穿衣服一样一层一层“穿上”和“脱下”的
		// 若此 IMC 的优先级更高，则会覆盖掉底层的开火、移动等操作
		if (Subsystem && TacticalIMC) Subsystem->AddMappingContext(TacticalIMC, 10);
	}

	// 如果判定为：准备关闭战术面板
	else
	{
		// 呼叫 UI 实例执行它自己的“反激活”逻辑（比如切换状态机、播放收起动画并在动画结束时自动出栈）
		TacticalWidgetInstance->OnWidgetDeactivated();

		// 从输入系统中剥夺战术面板专属的 IMC，把按键映射还给正常的 3D 游戏操作
		// RemoveMappingContext 卸载 IMC
		if (Subsystem && TacticalIMC) Subsystem->RemoveMappingContext(TacticalIMC);

		// 强制将引擎底层的“输入焦点”从 UI 身上剥离，并交还给 3D 游戏视口。这彻底解决了 UI 关闭后第一下鼠标点击无效的问题
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
	}
}
#pragma endregion

// ==============================================================================
// UI 统筹系统 (UI Management System)
// ==============================================================================
#pragma region
void ATopPlayerController::UpdateHUD()
{
	// 如果 UI 面板还未就绪，或者不在游戏世界里，直接返回
	if (!MainHUDInstance || !GetWorld()) return;

	// O(1) 极速调取：向当前 GameMode 索要已过滤好、最干净的存活友军名单
	if (AMyGameModeBase* GM = Cast<AMyGameModeBase>(GetWorld()->GetAuthGameMode()))
	{
		// 传递名单，并将当前玩家控制器所附身的 Pawn 转化为 ATopCharacter 作为当前活跃单位传入
		MainHUDInstance->UpdateSquadList(GM->FriendlyRoster, Cast<ATopCharacter>(GetPawn()));
	}
}
#pragma endregion

// ==============================================================================
// 战术指令与总线转发 (Tactical Commands & Bus Forwarding)
// ==============================================================================
#pragma region

bool ATopPlayerController::ProcessGlobalClick()
{
	/**
	 * 如果左键回调函数很多，就不能用这种 if 形式，而要考虑“状态机”或“责任链”
	 */

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
	if (TimeDilationHub) TimeDilationHub->SetSwitchMode(bEnable);
}

void ATopPlayerController::SwitchToSpecificCharacter(ATopCharacter* TargetCharacter)
{
	// 如果当前不在切人模式（未按 Tab 键），直接无视 UI 发来的换人请求
	if (!IsSwitchModeActive()) return;

	// 防御验证：目标为空，或者点的正是自己
	if (!TargetCharacter || TargetCharacter == CachedMyCharacter)
	{
		SetSwitchMode(false);

		// 只要鼠标点击过 UI，UI 就会霸占引擎的“输入焦点”
		// 这会导致你下一次按 Tab 键、按 WASD 键，全部被 UI 吞掉，游戏毫无反应
		// 这句话就是“上帝之手”，强行把焦点从 UI 剥夺，摔回给 3D 游戏画面
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
		return;
	}

	// 移交控制权
	Possess(TargetCharacter);

	// 换人成功后，自动退出切人模式
	SetSwitchMode(false);

	// 同上
	FSlateApplication::Get().SetAllUserFocusToGameViewport();
}
#pragma endregion
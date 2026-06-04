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
	// 1. 同步时空枢纽的子弹时间
	if (TimeDilationHub) TimeDilationHub->ToggleBulletTime();

	// 2. 懒加载可激活控件 
	if (!TacticalWidgetInstance && TacticalWidgetClass)
	{
		TacticalWidgetInstance = CreateWidget<UMyActivatableWidgetBase>(this, TacticalWidgetClass);
		if (TacticalWidgetInstance)
		{
			TacticalWidgetInstance->AddToViewport(100);
			TacticalWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);
			TacticalWidgetInstance->OnCloseRequested.AddUniqueDynamic(this, &ATopPlayerController::ToggleTacticalWidget);
		}
	}

	if (!TacticalWidgetInstance) return;

	auto* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());

	// 用高优先级 IMC 是否挂载，作为面板开关的“唯一真理”
	bool bIsActivating = Subsystem ? !Subsystem->HasMappingContext(TacticalIMC) : true;

	if (bIsActivating)
	{
		// 触发 UI 智能出场（会自动清空残留动画并满帧播放）
		TacticalWidgetInstance->OnWidgetActivated();

		FInputModeUIOnly ModeData;
		ModeData.SetWidgetToFocus(TacticalWidgetInstance->TakeWidget());
		SetInputMode(ModeData);
		bShowMouseCursor = true;

		// 挂载高优先级拦截沙箱（物理级屏蔽角色指令）
		if (Subsystem && TacticalIMC)
		{
			Subsystem->AddMappingContext(TacticalIMC, 10);
		}

		// ==============================================================================
		// 【防幽灵按键滑步】：强制清空硬件残留信号！
		// 解决长按 WASD 瞬间呼出菜单时，移动 IMC 收不到按键抬起信号导致的死锁滑行 Bug。
		// ==============================================================================
		FlushPressedKeys();
	}
	else
	{
		// 触发 UI 智能退场（自动计算打断补偿或进行镜像倒播）
		TacticalWidgetInstance->OnWidgetDeactivated();

		FInputModeGameAndUI GameModeData;
		GameModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		GameModeData.SetHideCursorDuringCapture(false);
		SetInputMode(GameModeData);
		bShowMouseCursor = true;

		// 卸载拦截沙箱，玩家操作瞬间恢复！
		if (Subsystem && TacticalIMC)
		{
			Subsystem->RemoveMappingContext(TacticalIMC);
		}

		// ==============================================================================
		// 【焦点零延迟重置】：物理级粉碎 UI 焦点路径！
		// 解决关闭菜单后，第一下点击鼠标可能没反应的体感粘滞 Bug。
		// ==============================================================================
		FSlateApplication::Get().ClearUserFocus(0);
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
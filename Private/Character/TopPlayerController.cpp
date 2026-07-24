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

// 【新增：屏幕文本输出所需的全局引擎头文件】
#include "Engine/Engine.h"

#include "Engine/GameViewportClient.h"

ATopPlayerController::ATopPlayerController()
{
	bReplicates = false;

	// ==============================================================================
	// 【核心修复：防 0x30 闪退的最高豁免权】
	// 强制声明为非空间加载！
	// 赋予控制器“外交豁免权”，防止它在 Standalone 独立进程下被 World Partition 强行吸入流送层，
	// 从而在 Seamless Travel 时被引擎当作流送垃圾销毁，引发空指针 0x30 崩溃。
	// ==============================================================================
	bIsSpatiallyLoaded = false;

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

	// ==============================================================================
	// 【核心破案：清除初始空间的幽灵控制器残留】
	// 初始关卡加载时，系统必定因为 GameMode 和 AutoPossess 的双重生成机制产生幽灵控制器。
	// 如果当前控制器没有本地玩家 (LocalPlayer) 附身，直接剥夺它身上时间枢纽的 Tick 权力！
	// 彻底终结初始空间的“双向拔河”！
	// ==============================================================================
	if (!IsLocalPlayerController())
	{
		if (TimeDilationHub)
		{
			TimeDilationHub->SetComponentTickEnabled(false);
		}
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
}

void ATopPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 【核武级修复】：因为项目启用了 Common UI 插件 (ActionRouter)
	// 绝对禁止在控制器销毁 (EndPlay) 阶段调用 SetInputMode 或强清 Subsystem！
	// 否则会将濒死的控制器塞入 Common UI 的全局树，导致点击停止游戏时触发 0xed0 内存越界闪退。
	// 之前的 Line 182 闪退已经被我们在 FlushPressedKeys 里的雷达完美拦截，这里无需再做多余操作。

	Super::EndPlay(EndPlayReason);
}

#pragma endregion

// ==============================================================================
// 灵魂附身与输入绑定 (Possession & Input Setup)
// ==============================================================================
#pragma region

void ATopPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	
	// 虚幻5.8：【最安全的全局底层搭桥地点】
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->ClearAllMappings();
		if (TopContext)
		{
			Subsystem->AddMappingContext(TopContext, 0);
		}
	}

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

	// ==============================================================================
	// 【核心修复：无缝传送后的“解封”与“焦点夺回”】
	// ==============================================================================
	if (IsLocalPlayerController())
	{
		// 1. 解除地图流转前制造的“全局视口输入黑洞”
		if (GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->SetIgnoreInput(false);
		}

		// 2. 解除控制器的软禁状态
		EnableInput(this);
		SetIgnoreMoveInput(false);
		SetIgnoreLookInput(false);

		// 3. 强行夺回操作系统的输入焦点（完美解决“必须点一下鼠标”的根本原因）
		FSlateApplication::Get().SetAllUserFocusToGameViewport();

		// 4. 重新应用一次输入模式，因为 BeginPlay 不会在无缝传送后再次执行！
		FInputModeGameAndUI InputModeData;
		InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputModeData.SetHideCursorDuringCapture(false);
		SetInputMode(InputModeData);
	}

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

	// ==========================================================
	// 虚幻5.8：【过河拆桥：灵魂离开旧肉体前，先把它身上的 WASD 卸载掉】
	if (CachedMyCharacter)
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			if (UInputMappingContext* OldContext = CachedMyCharacter->GetDefaultMappingContext())
			{
				Subsystem->RemoveMappingContext(OldContext);
			}
		}
	}
	// ==========================================================

	// 移交控制权：调用虚幻引擎底层的灵魂附身
	// 此操作会自动依次触发旧角色的 OnUnPossess 和新角色的 OnPossess
	Possess(TargetCharacter);

	// 换人成功后，自动退出切人模式（恢复正常时间流逝）
	if (IsSwitchModeActive()) SetSwitchMode(false);

	// 同上：确保附身新躯体后，玩家能立刻用键盘鼠标操控角色，而不是按键继续被 UI 吞噬
	FSlateApplication::Get().SetAllUserFocusToGameViewport();
}

#pragma endregion

// ==============================================================================
// 玩家 UI 交互契约实现 (Player UI Interface Implementation)
// ==============================================================================
#pragma region

void ATopPlayerController::ToggleSaveMenu_Implementation()
{
	if (UIHandlerComp)
	{
		UIHandlerComp->ToggleSaveMenuWidget();
	}
}

void ATopPlayerController::RequestCharacterSwitch_Implementation(ABaseCharacter* TargetCharacter)
{
	// 【找回被我私自删掉的逻辑】：状态防火墙！
	// 绝对不允许在“非切人模式”下响应任何 UI 的换人请求。
	// 拦截玩家在正常游戏流逝下，直接点击头像导致的“非法强切”。
	if (!IsSwitchModeActive())
	{
		// 防卡死：由于玩家鼠标点击了 UI，UI 底层机制会瞬间抢走输入焦点。
		// 此时既然我们拒绝了换人请求，就必须立刻把焦点一脚踢回给 3D 游戏视口，
		// 否则玩家接下来按 WASD 或空格键将会毫无反应！
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
		return;
	}

	// UI 通过契约点对点发来的请求，直接复用已有的完美切人逻辑
	if (ATopCharacter* CastedChar = Cast<ATopCharacter>(TargetCharacter))
	{
		SwitchToSpecificCharacter(CastedChar);
	}
}

#pragma endregion


// ==============================================================================
// 崩溃断点雷达 (Crash Diagnostic Radar)
// ==============================================================================
#pragma region

void ATopPlayerController::FlushPressedKeys()
{
	// 1. 抓取当前控制器的生死状态与世界指针
	bool bIsPendingKill = !IsValid(this); // UE5 中 IsValid 会检查是否为 PendingKill 或 Garbage
	UWorld* CurrentWorld = GetWorld();

	// 2. 只要该函数被引擎调用，立刻写入红色高警日志
	UE_LOG(LogTemp, Error, TEXT("========== [硬核雷达] 引擎底层触发了 FlushPressedKeys =========="));
	UE_LOG(LogTemp, Error, TEXT(">> 触发控制器: %s"), *GetName());
	UE_LOG(LogTemp, Error, TEXT(">> 该控制器是否已处于 PendingKill (被销毁但未回收): %d"), bIsPendingKill);
	UE_LOG(LogTemp, Error, TEXT(">> 该控制器的 World 指针是否为空: %d"), CurrentWorld == nullptr);

	if (CurrentWorld != nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT(">> 该物理世界是否已失效: %d"), !IsValid(CurrentWorld));
		// 【修正为真实的 UE 底层 API】
		UE_LOG(LogTemp, Error, TEXT(">> 该物理世界是否正在被销毁 (bIsTearingDown): %d"), CurrentWorld->bIsTearingDown);
	}

	// 3. 致命状态拦截报警 (融入了真实的真空期双重防御)
	if (bIsPendingKill || CurrentWorld == nullptr || !IsValid(CurrentWorld) || CurrentWorld->bIsTearingDown)
	{
		UE_LOG(LogTemp, Error, TEXT(">> [致命拦截] 抓到现行！引擎试图让一个已经死亡、失去 World 或处于切图销毁期的 Controller 去执行按键冲刷！"));
		UE_LOG(LogTemp, Error, TEXT(">> [致命拦截] 如果不在此处拦截，底层 PlayerInput 必定触发 Line 182 断言崩溃。本次已强行阻断！"));

		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 20.f, FColor::Red, TEXT("已成功拦截一次致命闪退！请查看 Saved/Logs 日志"));

		// 强行阻断，不让它调 Super
		return;
	}

	// 如果一切正常，放行给引擎底层处理
	Super::FlushPressedKeys();
}

#pragma endregion
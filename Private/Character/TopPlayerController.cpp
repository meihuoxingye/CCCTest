// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/TopPlayerController.h"

// 增强输入
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h" 

//角色
#include "Character/TopCharacter.h"

// 自定义移动控制组件
#include "Component/MovementControl/MyMovementControlComponent.h"

// 自定义战斗组件
#include "Component/CombatSystem/MyCombatComponent.h"

// 全局时空控制
#include "Kismet/GameplayStatics.h"

// 引入 UI 及 GameMode 头文件
#include "UI/MyMainHUDWidget.h"
#include "Game/MyGameModeBase.h"

// 必须包含这两个个来设置材质参数集合资产（MPC）
#include "Materials/MaterialParameterCollection.h" 
#include "Kismet/KismetMaterialLibrary.h"


#include "SwitchSubsystem/CharacterSwitchSubsystem.h"
// 解决 FSlateApplication 报红
#include "Framework/Application/SlateApplication.h" 

// 解决 FWidgetPath 报红
#include "Layout/WidgetPath.h"


ATopPlayerController::ATopPlayerController()
{
	bReplicates = false;
}

void ATopPlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UWorld* World = GetWorld();
	if (!World) return;

	// 一定要写 GlobalUIMPC.Get()，原生指针和原生指针一起判定
	if (GlobalUIMPC.Get())
	{
		UKismetMaterialLibrary::SetScalarParameterValue(
			GetWorld(),
			GlobalUIMPC.Get(),  // 原生指针
			FName("GlobalGameTime"),
			GetWorld()->GetTimeSeconds()
		);
	}


	// ===================== 【切人模式：平滑子弹时间】 =====================
	float CurrentDilation = UGameplayStatics::GetGlobalTimeDilation(World);

	if (!FMath::IsNearlyEqual(CurrentDilation, TargetTimeDilation, 0.005f))
	{
		float UnscaledDelta = DeltaTime / FMath::Max(0.001f, CurrentDilation);
		float NewDilation = FMath::FInterpTo(CurrentDilation, TargetTimeDilation, UnscaledDelta, 15.f);

		if (FMath::Abs(NewDilation - TargetTimeDilation) < 0.01f)
		{
			NewDilation = TargetTimeDilation;
		}

		UGameplayStatics::SetGlobalTimeDilation(World, NewDilation);
	}
}

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

void ATopPlayerController::SwitchToSpecificCharacter(ATopCharacter* TargetCharacter)
{
	// 如果当前不在切人模式（未按 Tab 键），直接无视 UI 发来的换人请求
	if (!bIsSwitchModeActive) return;

	// 防御验证：目标为空，或者点的正是自己
	if (!TargetCharacter || TargetCharacter == CachedMyCharacter)
	{
		SetSwitchMode(false);

		// 【新增】：哪怕没换人，也要强制夺回焦点，防止 Tab 键被 UI 吞掉
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
		return;
	}

	// 移交控制权
	Possess(TargetCharacter);

	// 换人成功后，自动退出切人模式
	SetSwitchMode(false);

	// 【新增核心修复】：强制将操作焦点从 UI 剥夺，还给 3D 游戏视口！
	// 这句代码会彻底打断 UI 对 Tab 键的劫持，让下一次按 Tab 键百分百触发你的子弹时间！
	FSlateApplication::Get().SetAllUserFocusToGameViewport();
}

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

	// 监听换人总线
	if (UCharacterSwitchSubsystem* SwitchSub = GetWorld()->GetSubsystem<UCharacterSwitchSubsystem>())
	{
		SwitchSub->OnSwitchRequest.AddUObject(this, &ATopPlayerController::SwitchToSpecificCharacter);
	}
}

void ATopPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EnhancedInputComponent = 
		CastChecked<UEnhancedInputComponent>(InputComponent);

	// 绑定回调
	if (EnhancedInputComponent)
	{
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ATopPlayerController::Move);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ATopPlayerController::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ATopPlayerController::StopJump);
		EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Started, this, &ATopPlayerController::Attack);
		EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Completed, this, &ATopPlayerController::AttackEnd);
		EnhancedInputComponent->BindAction(BulletTimeAction, ETriggerEvent::Completed, this, &ATopPlayerController::BulletTime);
		EnhancedInputComponent->BindAction(SwitchModeAction, ETriggerEvent::Started, this, &ATopPlayerController::ToggleSwitchMode);
	}

}

void ATopPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (!InPawn) return;

	#pragma region 缓存

	// 只找自定义输入移动组件这一次，然后缓存
	CachedMyMovementControlComp = InPawn->FindComponentByClass<UMyMovementControlComponent>();
	if (!CachedMyMovementControlComp) return;

	// 缓存角色
	CachedMyCharacter = Cast<ATopCharacter>(InPawn);
	if (!CachedMyCharacter) return;

	// 缓存自定义战斗组件
	CachedMyCombatComp = CachedMyCharacter->GetCombatComponent();

	#pragma endregion


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

	CachedMyMovementControlComp = nullptr;
	CachedMyCharacter = nullptr;
}

void ATopPlayerController::Move(const FInputActionValue& InputActionValue)
{
	// 移动输入动作是一个 Axis2D 类型，要获取 X 和 Y 轴数据
	// InputActionValue.Get,将键盘传入的数据转换为二维向量
	// 键盘 X 表示 A/D，键盘 Y 表示 W/S，其中 D、W 为正值
	const FVector2D InputAxisVector = InputActionValue.Get<FVector2D>();

	if (CachedMyMovementControlComp)
	{
		// 让组件去处理具体的移动逻辑
		CachedMyMovementControlComp->HandleMoveInput(InputAxisVector);
	}
}

void ATopPlayerController::Jump()
{
	if (CachedMyCharacter)
	{
		CachedMyCharacter->Jump();
	}
}

void ATopPlayerController::StopJump()
{
	if (CachedMyCharacter)
	{
		CachedMyCharacter->StopJumping();
	}
}

void ATopPlayerController::Attack()
{
	// ===================== 【终极双重防走火】 =====================
	bool bIsOverUI = false;

	if (UCharacterSwitchSubsystem* SwitchSub = GetWorld()->GetSubsystem<UCharacterSwitchSubsystem>())
	{
		bIsOverUI = SwitchSub->bIsPointerOverUI;
	}

	if (!bIsOverUI && FSlateApplication::IsInitialized())
	{
		FWidgetPath WidgetPath = FSlateApplication::Get().LocateWindowUnderMouse(
			FSlateApplication::Get().GetCursorPos(),
			FSlateApplication::Get().GetInteractiveTopLevelWindows()
		);

		if (WidgetPath.IsValid() && WidgetPath.Widgets.Num() > 0)
		{
			FString LeafWidgetName = WidgetPath.Widgets.Last().Widget->GetTypeAsString();
			if (LeafWidgetName != TEXT("SViewport"))
			{
				bIsOverUI = true;
			}
		}
	}

	if (bIsOverUI) return;
	// ==============================================================

	// 如果正处于切人模式，点击地面代表退出该模式，直接返回不开火
	if (bIsSwitchModeActive)
	{
		SetSwitchMode(false);
		return;
	}

	if (CachedMyCombatComp)
	{
		// 告诉战斗组件：开始射击
		CachedMyCombatComp->StartWeaponFire();
	}
}

void ATopPlayerController::AttackEnd()
{
	if (CachedMyCombatComp)
	{
		// 告诉战斗组件：停止射击
		CachedMyCombatComp->StopWeaponFire();
	}
}

void ATopPlayerController::BulletTime()
{
	UWorld* World = GetWorld();
	if (!World) return;

	if (bIsBulletTime == false)
	{
		bIsBulletTime = true;

		// 全局（DeltaTime）变慢 10 倍
		// UI 界面 走的是 RealTimeSeconds（绝对现实时间），天然对时间膨胀免疫
		UGameplayStatics::SetGlobalTimeDilation(World, 0.1f);

	}
	else
	{
		bIsBulletTime = false;
		// 时空速度恢复 1.0
		UGameplayStatics::SetGlobalTimeDilation(World, 1.0f);
	}
}

void ATopPlayerController::ToggleSwitchMode()
{
	SetSwitchMode(!bIsSwitchModeActive);
}

void ATopPlayerController::SetSwitchMode(bool bEnable)
{
	if (bIsSwitchModeActive == bEnable) return;

	bIsSwitchModeActive = bEnable;

	// 修改目标流速：开启切人模式时极慢动作，关闭时恢复正常
	TargetTimeDilation = bIsSwitchModeActive ? 0.05f : 1.0f;
}

// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/TopPlayerController.h"

// 增强输入
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h" 

//角色
#include "Character/TopCharacter.h"

// 自定义移动控制组件
#include "Component/MovementControl/MyMovementControlComponent.h"

// 异步线迹追踪子弹子系统
#include "Weapon/AsyncLineTraceBullet/MyBulletSubsystem.h"

// 自定义战斗组件
#include "Component/CombatSystem/MyCombatComponent.h"

ATopPlayerController::ATopPlayerController()
{
	bReplicates = false;
}

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
		EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Triggered, this, &ATopPlayerController::Attack);
	}

}

void ATopPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (InPawn)
	{
		// 只找自定义输入移动组件这一次，然后缓存
		CachedMyMovementControlComp = InPawn->FindComponentByClass<UMyMovementControlComponent>();

		// 缓存角色
		CachedMyCharacter = Cast<ACharacter>(InPawn);

		// 缓存自定义战斗组件
		MyCombatComp = InPawn->FindComponentByClass<UMyCombatComponent>();
	}
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
	MyCombatComp->ExecuteAttack();
}

void ATopPlayerController::AttackEnd()
{

}

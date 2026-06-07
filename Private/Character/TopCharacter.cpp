// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/TopCharacter.h"
// 自定义移动控制组件
#include "Component/MovementControl/MyMovementControlComponent.h"
// 引入对应的 GameMode 和控制器以触发 UI 刷新机制
#include "Game/MyGameModeBase.h"
#include "Character/TopPlayerController.h"
// 角色属性数据资产配置
#include "Character/CharacterAttributeDataAsset.h"
// 技能点子系统
#include "SkillSystem/SkillPointSubsystem.h"
// 使用了 GetWorld()，让编译器知道 UWorld* 指针
#include "Engine/World.h"
// 【新增模块】
#include "EnhancedInputComponent.h"
#include "Component/CombatSystem/MyCombatComponent.h"

// 【新增】：官方角色移动组件（为了修改 bRunPhysicsWithNoController）
#include "GameFramework/CharacterMovementComponent.h"


// Sets default values
ATopCharacter::ATopCharacter()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	MMCComponent = CreateDefaultSubobject<UMyMovementControlComponent>(TEXT("MyMovementControlComponent"));
}


// ==============================================================================
// 核心生命周期 (Core Lifecycle)
// ==============================================================================
#pragma region

// Called every frame
void ATopCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Called when the game starts or when spawned
void ATopCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 【新增核心修复】：允许没有控制器附身的躯壳继续受物理引擎控制（保持重力下落和惯性）
	// 这行代码会打破虚幻“无魂则静”的默认优化，彻底修复半空切人定格的 Bug！
	GetCharacterMovement()->bRunPhysicsWithNoController = true;

	// 1. 使用局部的 const 原始指针来接取配置资产（推荐的最佳实践）
	// 这样可以确保在当前的 BeginPlay 逻辑里，没有任何人能意外修改 Config 内部的数值
	const UCharacterAttributeDataAsset* Config = AttributeConfig.Get();
	if (!Config) return;

	// 2. 获取技能点子系统（注意：这里的子系统指针【绝对不能】加 const）
	if (USkillPointSubsystem* SkillPointSubsystem = GetWorld()->GetSubsystem<USkillPointSubsystem>())
	{
		// 3. 直接通过 const 指针读取成员并传入
		SkillPointSubsystem->AddNewCharacterToSquad(
			Config->CharacterID,
			Config->MaxSP,
			Config->InitialSP,
			Config->RegenRate
		);
	}


	// 类型为友好的角色出生自动注册与UI刷新调用
	if (Config && Config->CharacterType == ECharacterType::Friendly)
	{
		// GetAuthGameMode() 向当前关卡世界申请获取基础游戏模式
		if (AMyGameModeBase* GM = Cast<AMyGameModeBase>(GetWorld()->GetAuthGameMode()))
		{
			// 将此角色注册到 MyGameModeBase 的友军名册里
			GM->RegisterFriendly(this);

			// 通知玩家控制器更新 HUD 列表
			// GetFirstPlayerController() 获取0号（第一个）玩家控制器
			if (ATopPlayerController* PC = Cast<ATopPlayerController>(GetWorld()->GetFirstPlayerController()))
			{
				PC->UpdateHUD();
			}
		}
	}
}

void ATopCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 使用局部的 const 原始指针来接取配置资产（推荐的最佳实践）
	// 这样可以确保在当前的 BeginPlay 逻辑里，没有任何人能意外修改 Config 内部的数值
	const UCharacterAttributeDataAsset* Config = AttributeConfig.Get();
	if (!Config) return;


	// ===================== 【友军阵亡自动注销与UI刷新调用】 =====================
	if (Config && Config->CharacterType == ECharacterType::Friendly)
	{
		if (AMyGameModeBase* GM = Cast<AMyGameModeBase>(GetWorld()->GetAuthGameMode()))
		{
			GM->UnregisterFriendly(this);

			if (ATopPlayerController* PC = Cast<ATopPlayerController>(GetWorld()->GetFirstPlayerController()))
			{
				PC->UpdateHUD();
			}
		}
	}

	Super::EndPlay(EndPlayReason);
}

#pragma endregion


// ==============================================================================
// 玩家输入与行为绑定 (Player Input & Actions)
// ==============================================================================
#pragma region

// Called to bind functionality to input
void ATopCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);

	// 绑定回调
	if (EnhancedInputComponent)
	{
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ATopCharacter::Move);
		// 跳跃直接绑定基类 ACharacter 原生的 Jump 函数，极其精简
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Started, this, &ATopCharacter::Attack);
		EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Completed, this, &ATopCharacter::AttackEnd);
	}
}

void ATopCharacter::Move(const FInputActionValue& InputActionValue)
{
	// 移动输入动作是一个 Axis2D 类型，要获取 X 和 Y 轴数据
	// InputActionValue.Get,将键盘传入的数据转换为二维向量
	// 键盘 X 表示 A/D，键盘 Y 表示 W/S，其中 D、W 为正值
	const FVector2D InputAxisVector = InputActionValue.Get<FVector2D>();

	if (MMCComponent)
	{
		// 让组件去处理具体的移动逻辑
		MMCComponent->HandleMoveInput(InputAxisVector);
	}
}

void ATopCharacter::Attack()
{
	// 1. 玩家点下左键，先问大管家
	if (ATopPlayerController* PC = Cast<ATopPlayerController>(GetController()))
	{
		// 2. 如果此时 UI 开着，大管家会去关掉 UI，并返回 true
		if (PC->ProcessGlobalClick())
		{
			// 3. 核心在这里！如果大管家返回了 true，直接 return 结束这个函数！
			// 这意味着这一下鼠标点击“被吃掉了”，下面的开火代码根本不会执行！
			return;
		}
	}

	// 4. 只有当 UI 已经关干净了，再点下一次鼠标时，大管家才会返回 false
	// 代码才会一路畅通走到这里，执行真正的开火！
	if (MCComponent)
	{
		MCComponent->StartWeaponFire();
	}
}

void ATopCharacter::AttackEnd()
{
	if (MCComponent)
	{
		// 告诉战斗组件：停止射击
		MCComponent->StopWeaponFire();
	}
}

#pragma endregion
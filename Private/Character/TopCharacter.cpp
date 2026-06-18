// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/TopCharacter.h"
// 自定义移动控制组件
#include "Component/MovementControl/MyMovementControlComponent.h"
// 引入对应的 GameMode 和控制器以供数据注册与全局输入处理
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

// 【新增：交互系统所需的头文件】
#include "Components/SphereComponent.h"
#include "Interaction/MyInteractableInterface.h"
#include "Math/UnrealMathUtility.h"

// 【新增：屏幕文本输出所需的全局引擎头文件】
#include "Engine/Engine.h"

#include "Components/SkeletalMeshComponent.h"

// 虚幻5.8
// 【新增】：增强输入子系统与本地玩家，用于注册 IMC 桥梁
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"


// Sets default values
ATopCharacter::ATopCharacter()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	MMCComponent = CreateDefaultSubobject<UMyMovementControlComponent>(TEXT("MyMovementControlComponent"));

	// 【新增】：初始化交互探测球
	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(RootComponent);
	InteractionSphere->SetSphereRadius(150.f);
	InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionSphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
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

void ATopCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			// 清除可能残留的旧输入状态（非常重要）
			Subsystem->ClearAllMappings();

			if (DefaultMappingContext)
			{
				// 这一步就是把 [W] 映射到 MoveAction 的桥梁搭起来
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}
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

	// 类型为友好的角色出生自动注册
	if (Config && Config->CharacterType == ECharacterType::Friendly)
	{
		// GetAuthGameMode() 向当前关卡世界申请获取基础游戏模式
		if (AMyGameModeBase* GM = Cast<AMyGameModeBase>(GetWorld()->GetAuthGameMode()))
		{
			// 将此角色注册 to MyGameModeBase 的友军名册里
			// 角色现在只做纯粹的数据上报，后续的 UI 刷新将完全由 GameMode 的内部多播事件通知 UI 组件
			GM->RegisterFriendly(this);
		}
	}

	// 【新增】：绑定物理范围重叠委托
	if (InteractionSphere)
	{
		InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &ATopCharacter::OnInteractSphereBeginOverlap);
		InteractionSphere->OnComponentEndOverlap.AddDynamic(this, &ATopCharacter::OnInteractSphereEndOverlap);
	}
}

void ATopCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 使用局部的 const 原始指针来接取配置资产（推荐的最佳实践）
	// 这样可以确保在当前的 BeginPlay 逻辑里，没有任何人能意外修改 Config 内部的数值
	const UCharacterAttributeDataAsset* Config = AttributeConfig.Get();
	if (!Config) return;


	// ===================== 【友军阵亡自动注销】 =====================
	if (Config && Config->CharacterType == ECharacterType::Friendly)
	{
		if (AMyGameModeBase* GM = Cast<AMyGameModeBase>(GetWorld()->GetAuthGameMode()))
		{
			// 将此角色从友军名册中移除，名册内部的变动会自动触发整个 UI 视图层的刷新
			GM->UnregisterFriendly(this);
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

		// 【新增】：绑定交互按键
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &ATopCharacter::OnInteractKeyPressed);
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

// ==============================================================================
// 交互统筹系统 (Interaction Management System)
// ==============================================================================
#pragma region

void ATopCharacter::OnInteractSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && OtherActor != this && OtherActor->Implements<UMyInteractableInterface>())
	{
		InteractableActorsInRange.AddUnique(OtherActor);

		// 【测谎仪节点 1 - 绿色】：验证物理边界撞击以及接口识别
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("[雷达成功探测] 目标物体: %s 进入交互范围，且已成功识别交互接口契约！"), *OtherActor->GetName()));
		}
	}
}

void ATopCharacter::OnInteractSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (OtherActor)
	{
		InteractableActorsInRange.Remove(OtherActor);

		// 【测谎仪节点 2 - 红色】：验证物体离开
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("[雷达目标撤销] 目标物体: %s 离开了范围，已将其从待选队列移出。"), *OtherActor->GetName()));
		}
	}
}

AActor* ATopCharacter::GetClosestInteractableActor()
{
	if (InteractableActorsInRange.IsEmpty())
	{
		// 【测谎仪节点 3 - 橙色警告】：按键按下了，但列表居然是空的
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Orange, TEXT("[排查警告] 名单列表为空！雷达此时没有捕获到任何实现了接口的合法物体，请检查碰撞通道。"));
		}
		return nullptr;
	}

	AActor* BestActor = nullptr;
	double MinDistanceSq = MAX_dbl;
	int32 HighestPriority = MIN_int32;

	const FVector PlayerLocation = GetActorLocation();
	// 【核心修复】：改用骨骼网格体（视觉模型）的实际正前方
	FVector PlayerForward = GetMesh()->GetForwardVector();

	// 抹平 Z 轴，专为 2.5D 视角优化朝向判定
	PlayerForward.Z = 0.0;
	PlayerForward.Normalize();

	for (int32 i = InteractableActorsInRange.Num() - 1; i >= 0; --i)
	{
		AActor* CurrentActor = InteractableActorsInRange[i].Get();

		// 严密防御机制：剔除已被破坏或销毁的无效指针
		if (!IsValid(CurrentActor) || CurrentActor->IsActorBeingDestroyed())
		{
			InteractableActorsInRange.RemoveAt(i);
			continue;
		}

		// 朝向权重判定
		FVector DirToTarget = CurrentActor->GetActorLocation() - PlayerLocation;
		DirToTarget.Z = 0.0;
		DirToTarget.Normalize();

		const double DotWeight = FVector::DotProduct(PlayerForward, DirToTarget);

		// 过滤掉位于玩家背后（>90度）的物体
		if (DotWeight < 0.0)
		{
			// 【测谎仪节点 4 - 黄色信息】：物体在身边，但因为角度被点积算出来在背后，被无情过滤了
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, FString::Printf(TEXT("[排查提示] 发现了 %s，但它的点积结果为负数，判断在角色背后，已被剥夺交互权。"), *CurrentActor->GetName()));
			}
			continue;
		}

		// 优先级与距离演算
		const int32 CurrentPriority = IMyInteractableInterface::Execute_GetInteractionPriority(CurrentActor);
		const double DistanceSq = FVector::DistSquared(PlayerLocation, CurrentActor->GetActorLocation());

		if (CurrentPriority > HighestPriority)
		{
			HighestPriority = CurrentPriority;
			MinDistanceSq = DistanceSq;
			BestActor = CurrentActor;
		}
		else if (CurrentPriority == HighestPriority)
		{
			if (DistanceSq < MinDistanceSq)
			{
				MinDistanceSq = DistanceSq;
				BestActor = CurrentActor;
			}
		}
	}

	return BestActor;
}

void ATopCharacter::OnInteractKeyPressed()
{
	// 【测谎仪节点 5 - 浅蓝色】：验证增强输入到底有没有通到这个 C++ 函数里
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan, TEXT("[输入激活] 键盘 E 键已按下！增强输入底层管道打通，正在激活选品过滤算法..."));
	}

	// 只触发唯一的最佳目标，消除误操作
	if (AActor* TargetActor = GetClosestInteractableActor())
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, FString::Printf(TEXT("[锁定成功] 最终决出的最优目标是: %s！正在向其投掷 Execute_Interact 接口调用命令！"), *TargetActor->GetName()));
		}

		IMyInteractableInterface::Execute_Interact(TargetActor, this);
	}
}

#pragma endregion
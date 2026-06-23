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

// 虚幻5.8：引入全局输入映射上下文类
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

	// 【重回神坛：最安全的搭桥点】
	// 这里 100% 能拿到有效的 LocalPlayer，彻底告别开局失灵！
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 1);
			}
		}
	}

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
	// 【前置防线：空盘拦截】
	// 检查当前雷达缓存的数组里是否为空
	if (InteractableActorsInRange.IsEmpty())
	{
		// 【测谎仪节点 3 - 橙色警告】：按键按下了，但列表居然是空的
		// @排错意义：帮助开发者瞬间定位 Bug 是出在“按键没响应”还是“碰撞没配对”上。
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Orange, TEXT("[排查警告] 名单列表为空！雷达此时没有捕获到任何实现了接口的合法物体，请检查碰撞通道。"));
		}
		// 没东西可交互，直接下班
		return nullptr;
	}

	// 最终胜出的“交互对象”指针
	AActor* BestActor = nullptr;
	// 距离擂主的记录板：初始设为宇宙最大值，保证第一个上台的合法选手一定能打破这个记录
	double MinDistanceSq = MAX_dbl;
	// 优先级擂主的记录板：初始设为宇宙最小值，保证第一个上台的合法选手一定能打破这个记录
	int32 HighestPriority = MIN_int32;

	// 缓存主角当前的绝对物理坐标，作为测量所有选手距离的“靶心”
	const FVector PlayerLocation = GetActorLocation();

	// 【架构级细节：为什么要倒序（--i）？】
	// 因为我们在循环中可能会执行 RemoveAt(i) 踢人操作！
	// 如果是正序（0 -> N），踢掉 2 号位后，3 号位会自动滑到 2 号位，此时下一次循环直接去查 3 号位，
	// 就会导致那个滑过来的原本的“3 号位”被彻底漏检。倒序遍历完美规避了数组动态缩容导致的越界和漏检！
	for (int32 i = InteractableActorsInRange.Num() - 1; i >= 0; --i)
	{
		// 从弱指针（TWeakObjectPtr 或类似包装）中提取真实的 Actor 内存地址
		AActor* CurrentActor = InteractableActorsInRange[i].Get();

		// 【严密防御机制：清理僵尸指针】
		// 1. IsValid：防止指向的内存已经被 GC（垃圾回收）清理。
		// 2. IsActorBeingDestroyed：防止物体正在播放销毁动画、即将死亡，但还没被彻底清出内存。
		if (!IsValid(CurrentActor) || CurrentActor->IsActorBeingDestroyed())
		{
			// 如果碰到死人，直接把它从雷达名单里踢出去，防止下次按键时再查一遍
			InteractableActorsInRange.RemoveAt(i);
			// 跳过本次循环，换下一个选手上台
			continue;
		}

		// 1. 测算优先级：通过 UE 的接口系统（Interface），向当前选手发问：“你的交互优先级是多少？”
		// 使用 Execute_ 前缀是因为该接口大概率为 BlueprintNativeEvent，必须走虚幻的反射调用机制。
		const int32 CurrentPriority = IMyInteractableInterface::Execute_GetInteractionPriority(CurrentActor);

		// 2. 测算距离：计算主角和选手之间的【距离平方】。
		// 【性能压榨】：为什么用 DistSquared 而不是 Distance？
		// 算真实距离需要对坐标求算数平方根，CPU 开销极大。而比较远近大小，直接比平方值效果完全一样，这是 3A 级优化的基本素养。
		const double DistanceSq = FVector::DistSquared(PlayerLocation, CurrentActor->GetActorLocation());

		// 【第一层筛选：绝对阶级压制】
		// 如果当前选手的优先级，严格大于现在的擂主（比如任务 NPC 优先级高于地上的普通树枝）
		if (CurrentPriority > HighestPriority)
		{
			// 无视距离，直接篡位夺权，全面刷新三项擂主指标！
			HighestPriority = CurrentPriority;
			MinDistanceSq = DistanceSq;
			BestActor = CurrentActor;
		}
		// 【第二层筛选：平民夺魁机制】
		// 如果当前选手的优先级，跟现在的擂主一模一样大（比如地上有两根一模一样的树枝）
		else if (CurrentPriority == HighestPriority)
		{
			// 进入残酷的物理距离大比拼，如果当前选手离玩家更近...
			if (DistanceSq < MinDistanceSq)
			{
				// 篡位夺权，刷新距离和擂主身份（但不刷新优先级，因为大家一样大）
				MinDistanceSq = DistanceSq;
				BestActor = CurrentActor;
			}
		}
	}

	// 返回经历了千锤百炼后，最终留在台上的那个唯一胜者（如果全都挂了，这就是个 nullptr）
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
	// 调用筛选函数，获取当前范围内优先级最高且距离最近的交互目标
	if (AActor* TargetActor = GetClosestInteractableActor())
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, FString::Printf(TEXT("[锁定成功] 最终决出的最优目标是: %s！正在向其投掷 Execute_Interact 接口调用命令！"), *TargetActor->GetName()));
		}

		// 确认目标有效后，通过蓝图接口系统 (Interface) 通知目标执行具体的交互逻辑
		// 将玩家自身 (this) 作为交互的发起者 (Instigator) 传递给目标
		// 接收者 AMySavePointActor::Interact_Implementation
		IMyInteractableInterface::Execute_Interact(TargetActor, this);
	}
}

#pragma endregion
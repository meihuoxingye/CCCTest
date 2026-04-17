// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/Controller/MyAIController.h"

// 系统函数库，可用来调试打印
#include "Kismet/KismetSystemLibrary.h"

// 角色
#include "Character/BaseCharacter.h"

// 自定义战斗组件
#include "Component/CombatSystem/MyCombatComponent.h"

// 角色属性数据资产配置
#include "Character/CharacterAttributeDataAsset.h"

// 感知组件
#include "Perception/AIPerceptionComponent.h"             
// 视觉传感器配置
#include "Perception/AISenseConfig_Sight.h"        
// 基础感知数据结构，包含 FAIStimulus 数据结构
#include "Perception/AIPerceptionTypes.h"

// 组队子系统
#include "SquadUp/MySquadSubsystem.h"
// 规避工具
#include "Avoidance/MyAvoidanceUtils.h"

// 移动组件
#include "GameFramework/CharacterMovementComponent.h"

// 原生寻找路径
#include "NavigationSystem.h"
// 原生读取路标
#include "NavigationPath.h"

AMyAIController::AMyAIController()
{
	// 创建感知组件
	PerceptionComp = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("PerceptionComp"));

	// 创建感知配置
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));

	// 将感知配置应用到组件
	PerceptionComp->ConfigureSense(*SightConfig);
	PerceptionComp->SetDominantSense(SightConfig->GetSenseImplementation());

	// 绑定检测到后的回调函数
	PerceptionComp->OnTargetPerceptionUpdated.AddDynamic(this, &AMyAIController::OnTargetDetected);

	// 设置对敌人、友方和中立目标的检查
	// 实际上并不使用虚幻自带的阵营系统，而用自己设置的标签判断，但这三行仍必须要有
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
}

void AMyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	#pragma region 缓存报错区
	// 先尝试获取 Pawn，获取失败则退出并报错！
	if (!InPawn)
	{
		UE_LOG(LogTemp, Error, TEXT("感知到 Pawn[%s]，但获取失败"), *InPawn->GetName());
		return;
	}

	// 先尝试获取 Character，获取失败则退出并报错！
	if (!Cast<ABaseCharacter>(InPawn))
	{
		UE_LOG(LogTemp, Error, TEXT("感知到 Pawn[%s]，但转换为 Character 失败"), *InPawn->GetName());
		return;
	}

	// 先尝试获取属性资产配置，获取失败则退出并报错！
	if (!Cast<ABaseCharacter>(InPawn)->GetAttributeConfig())
	{
		UE_LOG(LogTemp, Error, TEXT("获取 Pawn[%s] 装换为的角色的属性资产配置失败"), *InPawn->GetName());
		return;
	}

	// 先尝试获取战斗组件，获取失败则退出并报错！
	if (!Cast<ABaseCharacter>(InPawn)->GetCombatComponent())
	{
		UE_LOG(LogTemp, Error, TEXT("获取 Pawn[%s] 装换为的角色的战斗组件失败"), *InPawn->GetName());
		return;
	}

	// 先尝试获取移动组件，获取失败则退出并报错！
	if (!Cast<ABaseCharacter>(InPawn)->GetCharacterMovement())
	{
		UE_LOG(LogTemp, Error, TEXT("获取 Pawn[%s] 装换为的角色的移动组件失败"), *InPawn->GetName());
		return;
	}
	#pragma endregion

	#pragma region 缓存区
	// AI 控制器不一定只控制角色
	CachedMyPawn = InPawn;

	CachedMyCharacter = Cast<ABaseCharacter>(InPawn);

	CachedMyCharacterConfig = CachedMyCharacter->GetAttributeConfig();
	SyncPerceptionProperties();

	CachedMyCombatComp = CachedMyCharacter->GetCombatComponent();

	CachedMovementComp = CachedMyCharacter->GetCharacterMovement();
	#pragma endregion

	// 将移动组件的移动模式改为 NavWalking，使角色不会离开导航网格
	CachedMovementComp->SetMovementMode(MOVE_NavWalking);
	// 当角色前方的地面高度差超过了最大上坡/下坡高度或者没有地面时，角色会停住
	CachedMovementComp->bCanWalkOffLedges = false;
}

void AMyAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!CachedMyCharacter) return;

	// 获取导航网格的全局管理器，用来调用各种原生的导航功能
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	// 获取小组子系统
	UMySquadSubsystem* SquadSub = GetWorld()->GetSubsystem<UMySquadSubsystem>();
	if (!NavSys || !SquadSub) return;

	// 1. 获取决策点
	FVector GoalLocation = SquadSub->GetTacticalLocation(CachedMyCharacter);

	// --- 【原生优化：降低寻路频率，提升灵敏度】 ---
	static FVector LastGoal;
	static FVector NextWaypoint;
	// 只有目标移动超过 50 厘米，才重新计算路径，否则沿用旧路点
	if (FVector::DistSquared(GoalLocation, LastGoal) > 2500.f)
	{
		UNavigationPath* NavPath = NavSys->FindPathToLocationSynchronously(this, CachedMyCharacter->GetActorLocation(), GoalLocation);
		if (NavPath && NavPath->PathPoints.Num() > 1)
		{
			NextWaypoint = NavPath->PathPoints[1];
		}
		LastGoal = GoalLocation;
	}

	// 2. 【你的避障逻辑】：只负责处理“活”的队友
	FVector DesiredDir = (NextWaypoint - CachedMyCharacter->GetActorLocation()).GetSafeNormal();
	float MaxSpeed = CachedMyCharacter->GetCharacterMovement()->MaxWalkSpeed;

	FVector ComputedVel = UMyAvoidanceUtils::CalculateAvoidanceVelocity(
		CachedMyCharacter->GetActorLocation(),
		CachedMyCharacter->GetVelocity(),
		DesiredDir * MaxSpeed,
		MaxSpeed,
		SquadSub->GetCandidates()
	);

	// 3. 执行位移
	CachedMyCharacter->AddMovementInput(ComputedVel.GetSafeNormal(), 1.0f);

	// --- 【原生边界保护：强制位置吸附】 ---
	// 这是原生 MoveTo 的核心：如果物理推力让你稍微出界了，瞬间把你拉回来
	FNavLocation NavLoc;
	// XY轴给100厘米容差，Z轴给500厘米（支持你要求的 Z 轴位移）
	if (NavSys->ProjectPointToNavigation(CachedMyCharacter->GetActorLocation(), NavLoc, FVector(100.f, 100.f, 500.f)))
	{
		// 关键：只修正 XY 轴，保留 Z 轴的物理（跳跃/下落）
		FVector CorrectedLoc = NavLoc.Location;
		CorrectedLoc.Z = CachedMyCharacter->GetActorLocation().Z;
		CachedMyCharacter->SetActorLocation(CorrectedLoc);
	}
}

void AMyAIController::OnUnPossess()
{
	Super::OnUnPossess();

	// 清空缓存
	CachedMyPawn = nullptr;
	CachedMyCharacter = nullptr;
	CachedMyCharacterConfig = nullptr;
	CachedMyCombatComp = nullptr;
}


void AMyAIController::OnTargetDetected(AActor* Actor, FAIStimulus Stimulus)
{
	if (!Actor) return;

	// 检查战斗组件、属性配置是否存在，且当前是看见目标还是跟丢目标
	// 看见目标
	if (CachedMyCombatComp && CachedMyCharacterConfig && Stimulus.WasSuccessfullySensed())
	{
		// 补充
		/**
		设置焦点与对这个交点的转向优先级
		若设置了角色移动组件里的 bUseControllerRotationYaw，那么角色就会自动转向焦点位置
		SetFocus(Actor,EAIFocusPriority::Gameplay);

		清除此优先级的焦点
		ClearFocus(EAIFocusPriority::Gameplay);

		但是这里有专用的旋转函数，不使用虚幻自带的
		*/


		// 是否为有效目标
		bool bIsValidTarget = false;

		// 遍历被控制的 Pawn 的检测目标类型数组
		for (ECharacterType TargetType : CachedMyCharacterConfig->TargetTypes)
		{
			FName TargetTagToLookFor;

			// 将枚举翻译为对应的 Tag
			switch (TargetType)
			{
			case ECharacterType::Friendly:	TargetTagToLookFor = FName("Friendly");	break;
			case ECharacterType::Enemy:		TargetTagToLookFor = FName("Enemy");	break;
			case ECharacterType::Neutral:	TargetTagToLookFor = FName("Neutral");	break;
			}

			// 检查进入视线的目标，身上有没有这个 Tag
			if (!TargetTagToLookFor.IsNone() && Actor->ActorHasTag(TargetTagToLookFor))
			{
				// 如果是有效目标
				bIsValidTarget = true;
				// 只要确认应该感知到这个进入视线的目标，就退出循环
				break; 
			}
		}

		// 如果感知到的目标属于被控制 Pawn 的目标类型数组，则开火
		if (bIsValidTarget)
		{
			CachedMyCombatComp->StartWeaponFire();

			// 【核心修改】通知小组：我们进入战斗状态（Aggro），开始移动！
			if (auto* SS = GetWorld()->GetSubsystem<UMySquadSubsystem>())
			{
				SS->SetGroupAggro(CachedMyCharacter, true);
			}
		}
	}

	// 跟丢目标
	else
	{
		// 告诉战斗组件：停火！被控制 Pawn 的战斗组件会自动去子系统里注销自己
		CachedMyCombatComp->StopWeaponFire();
	}
}

void AMyAIController::SyncPerceptionProperties()
{
	// 感知范围
	SightConfig->SightRadius = CachedMyCharacterConfig->DetectionRange;
	// 失去感知最大范围
	SightConfig->LoseSightRadius = CachedMyCharacterConfig->DetectionRange + 200.f;
	// 视角
	SightConfig->PeripheralVisionAngleDegrees = CachedMyCharacterConfig->VisionAngle;

	// 重启一次视觉频道，强制底层清空旧缓存并重新读取新数值
	if (PerceptionComp)
	{
		PerceptionComp->SetSenseEnabled(UAISense_Sight::StaticClass(), false);
		PerceptionComp->SetSenseEnabled(UAISense_Sight::StaticClass(), true);
	}
}
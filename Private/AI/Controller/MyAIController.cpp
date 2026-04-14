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

void AMyAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!CachedMyCharacter) return;

	UMySquadSubsystem* SquadSub = GetWorld()->GetSubsystem<UMySquadSubsystem>();
	if (!SquadSub) return;

	// 1. 获取战术期望：我要去哪？
	FVector TargetLoc = SquadSub->GetTacticalLocation(CachedMyCharacter);
	FVector DesiredDir = (TargetLoc - CachedMyCharacter->GetActorLocation()).GetSafeNormal();
	FVector DesiredVel = DesiredDir * CachedMyCharacter->GetCharacterMovement()->MaxWalkSpeed;

	// 2. 委托工具类计算避障：怎么不撞人地去那？
	// 传入当前状态和子系统里的全场候选人
	FVector ComputedVel = UMyAvoidanceUtils::CalculateAvoidanceVelocity(
		CachedMyCharacter->GetActorLocation(),
		CachedMyCharacter->GetVelocity(),
		DesiredVel,
		CachedMyCharacter->GetCharacterMovement()->MaxWalkSpeed,
		SquadSub->GetCandidates()
	);

	// 3. 执行最终位移
	CachedMyCharacter->AddMovementInput(ComputedVel.GetSafeNormal(), 1.0f);
}

void AMyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

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


	// AI 控制器不一定只控制角色
	CachedMyPawn = InPawn;

	CachedMyCharacter = Cast<ABaseCharacter>(InPawn);

	CachedMyCharacterConfig = CachedMyCharacter->GetAttributeConfig();
	SyncPerceptionProperties();

	CachedMyCombatComp = InPawn->FindComponentByClass<UMyCombatComponent>();
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
				bIsValidTarget = true;
				// 只要确认应该感知到这个进入视线的目标，就退出循环
				break; 
			}
		}

		// 如果感知到的目标属于被控制 Pawn 的目标类型数组，则开火
		if (bIsValidTarget)
		{
			CachedMyCombatComp->StartWeaponFire();
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
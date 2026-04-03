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

AMyAIController::AMyAIController()
{
	// 创建感知组件
	PerceptionComp = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("PerceptionComp"));

	// 创建感知配置
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));

	// 将配置应用到组件
	PerceptionComp->ConfigureSense(*SightConfig);
	PerceptionComp->SetDominantSense(SightConfig->GetSenseImplementation());

	// 绑定检测到后的回调函数
	PerceptionComp->OnTargetPerceptionUpdated.AddDynamic(this, &AMyAIController::OnTargetDetected);
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

	// 缓存被控制的 Pawn
	// AI 控制器不一定只控制角色
	CachedMyPawn = InPawn;

	// 缓存被控制的角色
	CachedMyCharacter = Cast<ABaseCharacter>(InPawn);

	// 缓存被控制的角色的属性资产配置
	CachedMyCharacterConfig = CachedMyCharacter->GetAttributeConfig();
	SyncPerceptionProperties();

	// 缓存自定义战斗组件
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
	// 检查战斗组件是否存在，且当前是“看见”目标还是“跟丢”目标
	if (CachedMyCombatComp && Stimulus.WasSuccessfullySensed())
	{
		/**
		设置焦点与对这个交点的转向优先级
		若设置了角色移动组件里的 bUseControllerRotationYaw，那么角色就会自动转向焦点位置
		SetFocus(Actor,EAIFocusPriority::Gameplay);

		清除此优先级的焦点
		ClearFocus(EAIFocusPriority::Gameplay);

		但是这里有专用的旋转函数，不使用虚幻自带的
		*/


		// 执行被感知目标的战斗组件的开火函数
		CachedMyCombatComp->ExecuteAttack();
	}
	else
	{
		
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
}
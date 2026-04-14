// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/BaseCharacter.h"

// 移动组件
#include "GameFramework/CharacterMovementComponent.h"

// 常用移动属性组件
#include "Component/MovementAttribute/MyMovementAttributeComponent.h"

// 自定义战斗组件
#include "Component/CombatSystem/MyCombatComponent.h"

// 基础武器类
#include "Weapon/MyWeaponBase.h"

// 角色属性数据资产配置
#include "Character/CharacterAttributeDataAsset.h"

// 刺激源组件，使此类型能够被感知系统识别到
#include "Perception/AIPerceptionStimuliSourceComponent.h"
// 视觉感官标识类，设置发送哪种信号给接收者（视觉 Sight、听觉 Hearing、伤害 Damage等）
#include "Perception/AISense_Sight.h"  

// 组队子系统
#include "SquadUp/MySquadSubsystem.h"

// Sets default values
ABaseCharacter::ABaseCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	MMAComponent = CreateDefaultSubobject<UMyMovementAttributeComponent>(TEXT("MyMovementAttributeComponent"));
	MCComponent = CreateDefaultSubobject<UMyCombatComponent>(TEXT("MyCombatComponent"));

	// 刺激源组件
	StimuliSourceComp = CreateDefaultSubobject<UAIPerceptionStimuliSourceComponent>(TEXT("StimuliSourceComp"));
	if (StimuliSourceComp)
	{
		// 把此类型发送的信号注册到视觉频道
		StimuliSourceComp->RegisterForSense(UAISense_Sight::StaticClass());
	}
}

// Called when the game starts or when spawned
void ABaseCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	// 禁用默认的“面向移动方向”
	GetCharacterMovement()->bOrientRotationToMovement = false;

	// 确保角色不随控制器（鼠标）的旋转而旋转
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	// 检查是否配置了武器类以及当前世界是否存在
	if (DefaultWeaponClass && GetWorld())
	{
		// 生成默认武器
		MCComponent->SpawnDefaultWeapon();
	}

	// 根据属性配置设置自身标签
	if (AttributeConfig)
	{
		FName MyTag;
		switch (AttributeConfig->CharacterType)
		{
		case ECharacterType::Friendly:	MyTag = FName("Friendly");	break;
		case ECharacterType::Enemy:		MyTag = FName("Enemy");		break;
		case ECharacterType::Neutral:	MyTag = FName("Neutral");	break;
		}

		// 将翻译好的 Tag 贴到自己身上
		this->Tags.AddUnique(MyTag);
	}


	// 只要配置里开启了组队功能，就去子系统注册，不分敌我
	if (AttributeConfig && AttributeConfig->bEnableSquadGrouping)
	{
		if (UMySquadSubsystem* SquadSub = GetWorld()->GetSubsystem<UMySquadSubsystem>())
		{
			SquadSub->RegisterCandidate(this); //
		}
	}
}

void ABaseCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 销毁时自动注销
	if (UMySquadSubsystem* SquadSub = GetWorld()->GetSubsystem<UMySquadSubsystem>())
	{
		SquadSub->UnregisterCandidate(this); //
	}
	
	// 2. 必须调用基类的 EndPlay（通常放在函数末尾）
	Super::EndPlay(EndPlayReason);
}


// Called every frame
void ABaseCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void ABaseCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

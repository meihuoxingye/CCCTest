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

// Sets default values
ABaseCharacter::ABaseCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	MMAComponent = CreateDefaultSubobject<UMyMovementAttributeComponent>(TEXT("MyMovementAttributeComponent"));
	MCComponent = CreateDefaultSubobject<UMyCombatComponent>(TEXT("MyCombatComponent"));
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

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

// Sets default values
ATopCharacter::ATopCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	MMCComponent = CreateDefaultSubobject<UMyMovementControlComponent>(TEXT("MyMovementControlComponent"));
}

// Called when the game starts or when spawned
void ATopCharacter::BeginPlay()
{
	Super::BeginPlay();

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

// Called every frame
void ATopCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void ATopCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}


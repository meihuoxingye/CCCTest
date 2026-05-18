// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/TopCharacter.h"
// 自定义移动控制组件
#include "Component/MovementControl/MyMovementControlComponent.h"

// 引入对应的 GameMode 和控制器以触发 UI 刷新机制
#include "Game/MyGameModeBase.h"
#include "Character/TopPlayerController.h"

// 角色属性数据资产配置
#include "Character/CharacterAttributeDataAsset.h"

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

	// ===================== 【新增：友军出生自动注册与UI刷新调用】 =====================
	if (AttributeConfig && AttributeConfig->CharacterType == ECharacterType::Friendly)
	{
		if (AMyGameModeBase* GM = Cast<AMyGameModeBase>(GetWorld()->GetAuthGameMode()))
		{
			GM->RegisterFriendly(this);

			// 通知玩家控制器更新 HUD 列表
			if (ATopPlayerController* PC = Cast<ATopPlayerController>(GetWorld()->GetFirstPlayerController()))
			{
				PC->UpdateHUD();
			}
		}
	}
}

void ATopCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// ===================== 【友军阵亡自动注销与UI刷新调用】 =====================
	if (AttributeConfig && AttributeConfig->CharacterType == ECharacterType::Friendly)
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


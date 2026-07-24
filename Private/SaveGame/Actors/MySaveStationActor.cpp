// Fill out your copyright notice in the Description page of Project Settings.

#include "SaveGame/Actors/MySaveStationActor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "UI/Subsystem/MyUIManagerSubsystem.h" 
#include "Blueprint/UserWidget.h"
#include "UI/ActivatableWidget/MyActivatableWidgetBase.h" 
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Character.h"
#include "Engine/Engine.h"
#include "Interaction/MyPlayerUIInterface.h"

// ==============================================================================
// 核心生命周期与初始化 (Core Lifecycle & Initialization)
// ==============================================================================
#pragma region

AMySaveStationActor::AMySaveStationActor()
{
	// 【性能优化】：关闭 Actor 的每帧 Tick 以节省 CPU 算力，存档点是静态物件无需每帧更新
	PrimaryActorTick.bCanEverTick = false;

	// 实例化静态网格体组件并将其命名为 StationMesh
	StationMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StationMesh"));

	// 将网格体设置为该 Actor 的根组件
	RootComponent = StationMesh;

	// 实例化盒体碰撞组件并将其命名为 TriggerBox
	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));

	// 将碰撞盒子附加到根组件上
	TriggerBox->SetupAttachment(RootComponent);

	// 【物理性能优化】：先将碰撞体的所有默认物理响应通道设置为忽略，避免不必要的底层物理检测开销
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);

	// 然后单独开启对 Pawn 通道的重叠响应，使其仅能检测到玩家角色的进入
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

#pragma endregion

// ==============================================================================
// 接口规范实现 (Interface Implementation)
// ==============================================================================
#pragma region

void AMySaveStationActor::Interact_Implementation(ACharacter* Interactor)
{
	// 安全性检查：拦截空指针，防止崩溃
	if (!Interactor) return;

	// 获取与该角色绑定的玩家控制器，准备发送 UI 指令
	if (APlayerController* PC = Cast<APlayerController>(Interactor->GetController()))
	{
		// 校验该控制器是否签署了 UI 空间契约，实现跨层级完全解耦
		if (PC->Implements<UMyPlayerUIInterface>())
		{
			// 通过契约接口盲发指令，命令控制器去翻转存档面板的状态
			IMyPlayerUIInterface::Execute_ToggleSaveMenu(PC);
		}
	}
}


FText AMySaveStationActor::GetInteractPrompt_Implementation() const
{
	// 返回本地化包装的静态提示字符串
	return FText::FromString(TEXT("链接记忆库"));
}

int32 AMySaveStationActor::GetInteractionPriority_Implementation() const
{
	// 赋予核心设施最高优先级 (如 10)，彻底碾压常规掉落物 (默认 0)
	return 10;
}

#pragma endregion
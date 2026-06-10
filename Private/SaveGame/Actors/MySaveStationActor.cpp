// Fill out your copyright notice in the Description page of Project Settings.

#include "SaveGame/Actors/MySaveStationActor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
// 确保包含你自己的 UI 管理器路径
#include "UI/Subsystem/MyUIManagerSubsystem.h" 
#include "Blueprint/UserWidget.h"
#include "UI/ActivatableWidget/MyActivatableWidgetBase.h" // 请根据你工程的实际路径确认
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"

// 【新增】：引入角色核心定义，消除 C2027 未定义类型错误
#include "GameFramework/Character.h"

// 【新增：日志组件】
#include "Engine/Engine.h"

// ==============================================================================
// 物理存档终端 (Physical Save Station Actor)
// ==============================================================================
#pragma region

AMySaveStationActor::AMySaveStationActor()
{
	PrimaryActorTick.bCanEverTick = false;

	StationMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StationMesh"));
	RootComponent = StationMesh;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	TriggerBox->SetupAttachment(RootComponent);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	// 将频道设置为 WorldDynamic 或直接使用 ECC_Pawn 进行重叠响应
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void AMySaveStationActor::Interact_Implementation(ACharacter* Interactor)
{
	if (!SaveMenuWidgetClass || !Interactor)
	{
		// 【测谎仪节点 6 - 红色致命报错】：终端收到指令了，但你忘在细节面板选图纸了！
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("[致命红字] 存档终端已被唤醒，但是由于 SaveMenuWidgetClass 为空 (None)，放弃 UI 实例化！请去蓝图细节面板指定 WBP_SaveMenu！"));
		}
		return;
	}

	// 【测谎仪节点 7 - 亮紫色】：接口命令成功跨河，送达物理 Actor 内部
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Magenta, TEXT("[业务接管] 物理存档终端已被调用！开始逆向层级反查本地控制器的 UMyUIManagerSubsystem..."));
	}

	// 获取玩家控制器
	if (APlayerController* PC = Cast<APlayerController>(Interactor->GetController()))
	{
		// 获取本地玩家
		if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
		{
			// 【关键】：从 LocalPlayer 身上获取 UI 子系统
			if (UMyUIManagerSubsystem* UISub = LocalPlayer->GetSubsystem<UMyUIManagerSubsystem>())
			{
				if (UMyActivatableWidgetBase* SpawnedMenu = CreateWidget<UMyActivatableWidgetBase>(GetWorld(), SaveMenuWidgetClass))
				{
					// 【测谎仪节点 8 - 亮紫色】：证明已经一脚把球踢进了 UI 管理子系统的最终大门
					if (GEngine)
					{
						GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Magenta, TEXT("[全链路打通] WBP_SaveMenu 实例创建成功，已提交推流数据：UISub->PushUI(SpawnedMenu)！"));
					}

					UISub->PushUI(SpawnedMenu);
				}
			}
			else
			{
				if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("[户籍报错] UMyUIManagerSubsystem 获取失败！它可能不是本地玩家子系统。"));
			}
		}
	}
}

FText AMySaveStationActor::GetInteractPrompt_Implementation() const
{
	return FText::FromString(TEXT("链接记忆库"));
}

int32 AMySaveStationActor::GetInteractionPriority_Implementation() const
{
	// 赋予核心设施最高优先级 (如 10)，彻底碾压常规掉落物 (默认 0)
	return 10;
}

#pragma endregion
// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/MyGameStateBase.h"
#include "Net/UnrealNetwork.h"

// ==============================================================================
// 核心生命周期与组件 (Core Lifecycle & Components)
// ==============================================================================
#pragma region
AMyGameStateBase::AMyGameStateBase()
{
	// 开启网络同步，允许 GameState 在全服存在并通信
	bReplicates = true;
}

void AMyGameStateBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 注册需要进行网络同步的属性
	// 这告诉引擎：“请将服务器上的 FriendlyRoster 数据实时下发给所有客户端”
	DOREPLIFETIME(AMyGameStateBase, FriendlyRoster);
}
#pragma endregion

// ==============================================================================
// 联机数据状态与同步 (Network Data & Replication)
// ==============================================================================
#pragma region
void AMyGameStateBase::OnRep_FriendlyRoster()
{
	// 当客户端底层的网络通道收到服务器发来的最新名单时，会被唤醒到这里。
	// 此时，我们只需拉响本地广播：
	OnRosterChanged.Broadcast();

	// 你的 MyUIHandlerComponent 听到广播后，就会纯本地去执行 UpdateHUD()
}
#pragma endregion
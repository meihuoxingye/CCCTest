// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "MyGameStateBase.generated.h"

class ATopCharacter;

// 声明一个动态多播委托，用于在友军名单发生变化时通知所有 UI 组件
// 作用：彻底解耦网络层与表现层。让 UI 或其他系统能以“盲听”的方式订阅友军名单变化，防止直接调指针引发空指针闪退。
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRosterChangedSignature);

/**
 * 【联机架构解析：全服公共揭示板 (The Public Scoreboard)】
 * 1. 架构定位：在多人联机游戏中，GameMode（裁判）仅存在于服务器内存中，客户端电脑上根本不存在。
 *    而 GameState 则是服务器摆在所有玩家面前的“大屏幕”，它会自动被克隆（Replicate）到每一个连入房间的客户端电脑上。
 * 2. 核心职责：专门用于存储需要【全服所有玩家共同知晓的全局状态数据】。
 *    例如：当前存活的友军名册 (FriendlyRoster)、比赛剩余时间、当前游戏阶段（准备、战斗、结算）、队伍总分数等。
 * 3. 权限与安全边界：客户端（普通玩家）只有权限“看”大屏幕上的数据，绝对没有权限直接修改 GameState 里的变量。
 *    任何试图改变游戏状态的行为（如投票踢人、上交任务物品），都必须先向服务器发送 RPC 请求，由服务器审核并修改 GameState。
 */
UCLASS()
class CCC_API AMyGameStateBase : public AGameStateBase
{
	GENERATED_BODY()

	// ==============================================================================
	// 核心生命周期与组件 (Core Lifecycle & Components)
	// ==============================================================================
public:
	AMyGameStateBase();

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;


	// ==============================================================================
	// 联机数据状态与同步 (Network Data & Replication)
	// ==============================================================================
public:
	// 💥 实体广播频道（大喇叭）。
	// 触发时机：1. 客机底层接收到新网络数据，触发 OnRep_FriendlyRoster 时；2. 房主在 GameMode 修改友军名单后手动拉响时。
	// 唤醒目标：自动跨文件调用 UMyUIHandlerComponent::UpdateHUD()。
	// 执行结果：UI 统筹组件拿着最新的 FriendlyRoster 数组去刷新画面（如小队存活头像）。
	UPROPERTY(BlueprintAssignable, Category = "GameState|Events")
	FOnRosterChangedSignature OnRosterChanged;

	// 友军名册
	// 💥【架构说明】ReplicatedUsing = OnRep_FriendlyRoster 的意思是：
	// 当服务器修改了这个数组并同步到客户端时，客户端会自动执行 OnRep_FriendlyRoster 这个函数。
	UPROPERTY(ReplicatedUsing = OnRep_FriendlyRoster, BlueprintReadOnly, Category = "GameState|Data")
	TArray<ATopCharacter*> FriendlyRoster;

protected:
	// 网络数据到达客户端时的自动回调函数
	// 注意：必须加 UFUNCTION()，否则底层的 ReplicatedUsing 找不到它
	UFUNCTION()
	virtual void OnRep_FriendlyRoster();
};
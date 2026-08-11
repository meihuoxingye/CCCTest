// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MyMapTravelNetComponent.generated.h"

/** 传送同步状态 (Teleport Sync State) */
UENUM()
enum class ETravelSyncState : uint8
{
	Idle,               // 空闲状态
	WaitingForClientUI, // 服务器已发令，等待客户端拉起黑幕并预热数据层
	Ready               // 客户端已就绪，黑屏完全闭合，等待服务器进行物理搬运
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class CCC_API UMyMapTravelNetComponent : public UActorComponent
{
	GENERATED_BODY()

	// ==============================================================================
	// 核心生命周期与组件 (Core Lifecycle & Components)
	// ==============================================================================
public:
	UMyMapTravelNetComponent();

protected:
	// 标记该组件参与复制，确保网络状态机能在主客机之间正确同步
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ==============================================================================
	// 核心网络握手管线 (Core Network Handshake)
	// ==============================================================================
public:
	/** [Server] 服务器发起同图同步指令，要求对应客户端拉起转场 UI */
	void Server_InitiateTransition(const FTransform& TargetTransform);

	/** [Client] 客户端接收指令：本地拉起黑幕 UI，挂起输入，并预热目标数据层 */
	UFUNCTION(Client, Reliable)
	void Client_HandleTransitionRequest(FTransform TargetTransform);

	/** [Server] 客户端确认就绪：黑幕已完全闭合，向服务器提交通行证 */
	UFUNCTION(Server, Reliable)
	void Server_NotifyClientReady();

	/** [Client] 服务器物理搬运完毕，下发解封令准许客户端卸载黑幕并亮屏 */
	UFUNCTION(Client, Reliable)
	void Client_NotifyPhysicalTeleportDone();

	/** 获取当前同步状态，供传送子系统的定时器轮询使用 */
	ETravelSyncState GetSyncState() const { return CurrentState; }

	/** 传送全流程结束后重置同步状态，为下一次传送做准备 */
	void ResetSyncState() { CurrentState = ETravelSyncState::Idle; }

private:
	// 核心状态机，必须开启网络复制以保证状态在双端的一致性
	UPROPERTY(Replicated)
	ETravelSyncState CurrentState = ETravelSyncState::Idle;

	// ==============================================================================
	// 跨地图专属握手管线 (Cross-Map Handshake)
	// ==============================================================================
public:
	// 服务器发令：跨地图专属握手指令
	// 说明：服务器调用此函数，内部会触发下方的 Client_ 宏函数进行 RPC 通信
	void Server_InitiateCrossMapTransition(FName TargetMapName);

	// 客户端接收：跨图表现层统筹
	// 【防坑必读】：在头文件声明 RPC 函数时，绝对不能加 _Implementation 后缀！
	// UFUNCTION 宏会让虚幻底层 (UHT) 自动生成原名函数用于网络数据打包。
	UFUNCTION(Client, Reliable)
	void Client_HandleCrossMapRequest(FName TargetMapName);
};
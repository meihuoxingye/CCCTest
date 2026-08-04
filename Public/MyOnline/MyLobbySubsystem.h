#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Online/Lobbies.h"

// 【新增】：引入你的路由数据资产，真正实现数据驱动
class UTeleportRoute;

class ULobbyConfigAsset;


#include "MyLobbySubsystem.generated.h"

UCLASS()
class CCC_API UMyLobbySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

	// ==============================================================================
	// 核心生命周期与组件 (Core Lifecycle & Components)
	// ==============================================================================
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ==============================================================================
	// 现代化联机大厅 (OSSv2 Lobbies)
	// ==============================================================================
public:
	// 主机：动作 1 —— 创建大厅（仅在 Epic 后端建立纯数据房间，绝不切图）
	// 【修改】：接收大厅配置数据资产
	UFUNCTION(BlueprintCallable, Category = "EOS|Lobby")
	void CreateEOSLobby(ULobbyConfigAsset* LobbyConfig);

	// 主机：动作 2 —— 房主切图开大门
	// 【核心复用】：直接接收你的路由数据资产 (UTeleportRoute)，拒绝手动传字符串！
	UFUNCTION(BlueprintCallable, Category = "EOS|Lobby")
	void StartEOSGame(UTeleportRoute* TargetRoute);

	// 副机：动作 3 —— 寻找大厅
	UFUNCTION(BlueprintCallable, Category = "EOS|Lobby")
	void FindEOSLobbies();

	// 副机：加入大厅并通过 P2P 隧道穿梭（被 FindEOSLobbies 自动调用）
	void JoinEOSLobby(const UE::Online::FLobbyId& LobbyIdToJoin);

protected:
	// 缓存当前加入的大厅 ID，用于后续的状态更新或退出
	UE::Online::FLobbyId CurrentLobbyId;
};
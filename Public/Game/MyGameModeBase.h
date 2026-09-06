// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "MapTravel/Actors/MyUniversalDestination.h" // 记得引入头文件

#include "MyGameModeBase.generated.h"


/**
 * */
UCLASS()
class CCC_API AMyGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

	// ==============================================================================
	// 生命周期 (Lifecycle)
	// ==============================================================================
public:
	// 声明构造函数
	AMyGameModeBase();

	// 【新增】：重写 InitGame，用于提前截胡数据层流送
	// 刚把地图文件读进内存，所有 Actor 都还没执行 BeginPlay，且绝对没有任何玩家能够连入的“第一毫秒”，引擎底层全自动调用此虚函数
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

	// 重写 StartPlay，作为后台基建预热的触发点
	virtual void StartPlay() override;

	// 重写 EndPlay，作为关卡销毁前的最后一道防线
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;


protected:
	// 拦截生命周期第一帧，移交数据层压制权
	virtual void BeginPlay() override;


	// ==============================================================================
	// 队伍名册系统 (Squad Roster System)
	// ==============================================================================
public:
	// 💥【修改说明】彻底删除了原有的 FriendlyRoster 数组和 OnRosterChanged 委托。
	// 这些数据已经被转移到了 MyGameStateBase 中进行全服同步。

	// 外部调用：角色出生时注册
	void RegisterFriendly(class ATopCharacter* Character);

	// 外部调用：角色死亡或销毁时注销
	void UnregisterFriendly(class ATopCharacter* Character);


	// ==============================================================================
	// 无缝旅行与状态流转 (Seamless Travel)
	// ==============================================================================
public:
	/** 重写：决定哪些正在飞行的“幽灵 Actor”能无视空间毁灭，直接跟入下一个世界 */
	virtual void GetSeamlessTravelActorList(bool bToTransition, TArray<AActor*>& ActorList) override;


	// ==============================================================================
	// 联机底层探针与接客管线 (Network Probes & Player Spawning)
	// ==============================================================================
public:
	// 第 1 步：客机彻底连入房间，向服务器报到
	// 当客机的网络驱动器完成了底层的 TCP/UDP 握手，通过了权限验证 (PreLogin/Login)
	// 并且引擎已经在服务器内存里真正生成了属于这个客机的灵魂 (PlayerController) 后，引擎全自动调用此虚函数
	virtual void PostLogin(APlayerController* NewPlayer) override;

	// 第 2 步：服务器准备为玩家的灵魂分配肉体 
	// 【核心重构】：已彻底褫夺引擎底层的自动生成权！
	// 现在的逻辑为 100% 绝对受控：无缝原肉体放行 -> 中途入场夺舍 -> 查表读取图纸 -> 截取接机点2.5D朝向 -> 手动捏人！
	virtual void RestartPlayer(AController* NewPlayer) override;

	// 【架构解耦接口】：暴露给外部子系统调用，用于执行受保护的底层 FinishRestartPlayer
	void ExecuteFinishRestartPlayer(AController* NewPlayer, const FRotator& StartRotation);

	// 离线收尾流程：处理玩家断线退出，让 AI 重新接管躯壳继续战斗
	// 现状说明：目前没有友军 AI，玩家断线退出后，躯壳会变成无人控制的空壳留在原地。
	// 预留说明：未来有了友军 AI，可以在这里解开注释，重新生成一个 AI 大脑塞进去，让它继续打。
	virtual void Logout(AController* Exiting) override;

private:
	// 记录本局游戏是否为开荒状态 (由 InitGame 源头判定)
	bool bIsInitialBoot = false;


	// ==============================================================================
	// 环境与数据层管控 (Environment & Data Layers)
	// ==============================================================================
public:
	// 在 GameMode 蓝图中，将创建好的 DL_StarterCharacters 资源拖入此处
	UPROPERTY(EditAnywhere, Category = "DataLayerAsset")
	TObjectPtr<const UDataLayerAsset> StarterDataLayer;
};
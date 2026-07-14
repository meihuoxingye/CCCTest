// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MyGameModeBase.generated.h"

// 定义一个多播委托，当友军名册发生变动时广播信号
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRosterChangedSignature);

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

	// 重写 StartPlay，作为后台基建预热的触发点
	virtual void StartPlay() override;

	// 重写 EndPlay，作为关卡销毁前的最后一道防线
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;


	// ==============================================================================
	// 队伍名册系统 (Squad Roster System)
	// ==============================================================================
public:
	// 动态维护当前存活友军的轻量级指针数组
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Roster")
	TArray<class ATopCharacter*> FriendlyRoster;

	// 名册变动广播频道
	UPROPERTY(BlueprintAssignable, Category = "Roster|Event")
	FOnRosterChangedSignature OnRosterChanged;

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

	// 覆写原生出生点规则：如果手里捏着跨界车票，强制覆盖玩家出生地
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
};
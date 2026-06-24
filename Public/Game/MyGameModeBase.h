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
};
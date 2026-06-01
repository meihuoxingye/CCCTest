// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CharacterSwitchSubsystem.generated.h"

class ATopCharacter;

// 1. 换人请求委托：UI 点击时广播，控制器接收
DECLARE_MULTICAST_DELEGATE_OneParam(FOnSwitchCharacterRequestSignature, ATopCharacter* /*TargetCharacter*/);

// 2. 主控角色变更委托：换人成功后广播，所有 UI 接收以刷新高亮
DECLARE_MULTICAST_DELEGATE_OneParam(FOnActiveCharacterChangedSignature, ATopCharacter* /*NewActiveChar*/);

UCLASS()
class CCC_API UCharacterSwitchSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// 换人频道的双向总线
	FOnSwitchCharacterRequestSignature OnSwitchRequest;
	FOnActiveCharacterChangedSignature OnActiveCharacterChanged;

	// 交互鲁棒性：鼠标是否正悬停在任何换人 UI 上的全局标志位
	bool bIsPointerOverUI = false;
};
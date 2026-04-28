// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MySquadMovementSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class CCC_API UMySquadMovementSubsystem : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()
	
public:
	FVector GetTacticalLocation(class ABaseCharacter* Character);

	// --- FTickableGameObject 接口实现 (新增以下两行) ---
	virtual void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UMySquadMovementSubsystem, STATGROUP_Tickables); }

private:
	// 
	void UpdateMovementLogic(float DeltaTime);
};

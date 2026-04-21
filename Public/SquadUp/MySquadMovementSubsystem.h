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

	virtual void Tick(float DeltaTime) override;

private:
	// 
	void UpdateMovementLogic(float DeltaTime);

	/** * 计算指定小组所有成员的几何中心点（重心） */
	FVector GetGroupCenter(const class FSquadGroup& Group) const;
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "MyAIController.generated.h"

/**
 * 
 */
UCLASS()
class CCC_API AMyAIController : public AAIController
{
	GENERATED_BODY()
	
public:
	AMyAIController();

protected:
	// 当目标被检测到
	void OnTargetDetected(AActor* Actor, struct FAIStimulus Stimulus);

	// 感知组件
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class UAIPerceptionComponent> PerceptionComp;

	// 感知配置
	UPROPERTY()
	TObjectPtr<class UAISenseConfig_Sight> SightConfig;
private:
	// 缓存 Pawn 指针
	UPROPERTY()
	TObjectPtr<APawn> CachedMyPawn;

	// 缓存角色指针
	UPROPERTY()
	TObjectPtr<class ABaseCharacter> CachedMyCharacter;

	// 缓存角色属性数据资产配置指针
	UPROPERTY()
	TObjectPtr<const class UCharacterAttributeDataAsset> CachedMyCharacterConfig;

	// 缓存被感知目标的战斗组件指针
	UPROPERTY()
	TObjectPtr<class UMyCombatComponent> CachedMyCombatComp;


	// 当 AI 控制器开始操控时执行
	virtual void OnPossess(APawn* InPawn) override;
	// 当控制器不再控制时执行
	virtual void OnUnPossess() override;

	// 同步自定义感知属性
	void SyncPerceptionProperties();
	
};

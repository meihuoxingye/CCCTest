// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SkillPointSubsystem.generated.h"

/**
 * 
 */

USTRUCT(BlueprintType)
struct FCharacterSPData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SP")
    float MaxSP = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SP")
    float SavedSP = 30.0f;

    UPROPERTY(BlueprintReadWrite, Category = "SP")
    double LastSyncGameTime = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SP")
    float RegenRate = 10.0f;

    UPROPERTY(BlueprintReadWrite, Category = "SP")
    bool bIsRegenFrozen = false;
};



UCLASS()
class CCC_API USkillPointSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    UPROPERTY(BlueprintReadWrite, Category = "Squad")
    TMap<FName, FCharacterSPData> SquadSPMap;

    // --- 核心只读函数，全部标记为 const，实现 Const Correctness ---

    UFUNCTION(BlueprintPure, Category = "Squad")
    float GetCharacterSP(FName CharacterID) const;

    UFUNCTION(BlueprintPure, Category = "Squad")
    int32 GetCurrentSPAsInt(FName CharacterID) const;

    // --- 修改状态的函数，保持非 const ---

    UFUNCTION(BlueprintCallable, Category = "Squad")
    bool ConsumeCharacterSP(FName CharacterID, float Amount);

    UFUNCTION(BlueprintCallable, Category = "Squad")
    void SetCharacterRegenFrozen(FName CharacterID, bool bFreeze);

    UFUNCTION(BlueprintCallable, Category = "Squad")
    void SetAllCharactersRegenFrozen(bool bFreeze);

    UFUNCTION(BlueprintCallable, Category = "Squad|SaveSystem")
    void PrepareForSave();

    UFUNCTION(BlueprintCallable, Category = "Squad|SaveSystem")
    void PostLoadSync();

};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
// 【必须添加】：C++23 强制要求显式包含
#include "Engine/Texture2D.h" 
#include "CharacterAttributeDataAsset.generated.h"

// 自定义枚举类，确定是友方、敌人还是中立
UENUM(BlueprintType)
enum class ECharacterType : uint8
{
	// 友方
	Friendly	UMETA(DisplayName = "Friendly"),
	// 敌人
	Enemy	UMETA(DisplayName = "Enemy"),
	// 中立
	Neutral	UMETA(DisplayName = "Neutral")
};

// AI 检测等级
// 自定义枚举类，确定敌人是无感知、短距离感知还是远距离感知
UENUM(BlueprintType)
enum class EAIDetectionLevel : uint8
{
	// 无感知
	// 避免使用 None，它在虚幻里有特殊含义
	NoPerception	UMETA(DisplayName = "No Perception"),
	// 短距离
	ShortRange	UMETA(DisplayName = "Short Range"),
	// 远距离 
	LongRange	UMETA(DisplayName = "Long Range")
};

UCLASS()
class CCC_API UCharacterAttributeDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	// 角色类型，枚举类
	UPROPERTY(EditAnywhere, Category = "Logic")
	ECharacterType CharacterType = ECharacterType::Friendly;

	// 角色的专属头像（软引用方式）
	// meta 语法解析：只有当 CharacterType 变量的值等于 ECharacterType::Friendly 时，该属性才允许编辑；若不等于，则直接在细节面板隐藏！
	UPROPERTY(EditAnywhere, Category = "Visuals", meta = (EditCondition = "CharacterType == ECharacterType::Friendly", EditConditionHides))
	TSoftObjectPtr<UTexture2D> CharacterAvatar;

	// 角色 ID
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Logic")
	FName CharacterID = FName("DefaultCharacter");

	// AI 检测等级，枚举类
	UPROPERTY(EditAnywhere, Category = "Logic")
	EAIDetectionLevel AIDetectionLevel = EAIDetectionLevel::NoPerception;

	// AI 将检测的目标类型，数组形式
	UPROPERTY(EditAnywhere, Category = "Logic", meta = (EditCondition = "AIDetectionLevel != EAIDetectionLevel::NoPerception", EditConditionHides))
	TArray<ECharacterType> TargetTypes;

	// 最大生命值
	// 当前生命值应在对应类的成员变量里
	// 因为 DataAsset 在内存中只有一份，写在这里全地图所有的同种角色都会同时掉血
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base Attribute")
	float MaxHealth = 100.f;

	// 最大技能点数
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base Attribute")
	float MaxSP = 100.0f;

	// 初始技能点数
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base Attribute")
	float InitialSP = 0.0f;

	// 技能点恢复速度
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base Attribute")
	float RegenRate = 10.0f;


	#pragma region AI_Perception
	// 检测范围
	// 有感知才会显示此项
	UPROPERTY(EditAnywhere, Category = "AIPerception", meta = (EditCondition = "CharacterType == ECharacterType::Enemy && AIDetectionLevel != EAIDetectionLevel::NoPerception", EditConditionHides))
	float DetectionRange = 1200.f;

	// 视角角度
	// 有感知才会显示此项
	UPROPERTY(EditAnywhere, Category = "AIPerception", meta = (EditCondition = "CharacterType == ECharacterType::Enemy && AIDetectionLevel != EAIDetectionLevel::NoPerception", EditConditionHides))
	float VisionAngle = 60.f;
	#pragma endregion


	// --- 组队功能配置 ---
	// 是否启用动态组队功能
	UPROPERTY(EditAnywhere, Category = "Squad")
	bool bEnableSquadGrouping = false; 

	// 感应队友的半径
	UPROPERTY(EditAnywhere, Category = "Squad", meta = (EditCondition = "bEnableSquadGrouping"))
	float GroupingRadius = 800.f; 

	// 该类角色最小带队人数
	UPROPERTY(EditAnywhere, Category = "Squad", meta = (EditCondition = "bEnableSquadGrouping"))
	int32 MinSquadCapacity = 2;
	// 该类角色最大带队人数
	UPROPERTY(EditAnywhere, Category = "Squad", meta = (EditCondition = "bEnableSquadGrouping"))
	int32 MaxSquadCapacity = 5;

	// 根据上下限随机化带队人数
	// 加上 BlueprintCallable 方便在编辑器里也能调
	UFUNCTION(BlueprintPure, Category = "Squad")
	int32 GetRandomSquadSize() const
	{
		return FMath::RandRange(MinSquadCapacity, MaxSquadCapacity);
	}

	#pragma region 常用移动属性
	UPROPERTY(EditAnywhere, Category = "Common Movement Properties")
	float MaxWalkSpeed = 600.f;

	UPROPERTY(EditAnywhere, Category = "Common Movement Properties")
	float MaxAcceleration = 2048.f;

	// 停止移动时的减速能力
	UPROPERTY(EditAnywhere, Category = "Common Movement Properties")
	float MoveDeceleration = 2048.f;

	UPROPERTY(EditAnywhere, Category = "Common Movement Properties")
	float JumpSpeed = 700.f;

	// 空中方向控制力
	UPROPERTY(EditAnywhere, Category = "Common Movement Properties")
	float AirControl = 0.2f;

	// 重力缩放
	UPROPERTY(EditAnywhere, Category = "Common Movement Properties")
	float GravityScale = 1.0f;
	#pragma endregion 常用移动属性
};

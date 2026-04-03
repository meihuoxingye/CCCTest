// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CharacterAttributeDataAsset.generated.h"

// 自定义枚举类，确定是玩家还是敌人
UENUM(BlueprintType)
enum class ECharacterType : uint8
{
	// 玩家
	Player	UMETA(DisplayName = "Player"),
	// 敌人
	Enemy	UMETA(DisplayName = "Enemy")
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
	ECharacterType CharacterType = ECharacterType::Player;

	// AI 检测等级，枚举类
	UPROPERTY(EditAnywhere, Category = "Logic")
	EAIDetectionLevel AIDetectionLevel = EAIDetectionLevel::NoPerception;

	// 最大生命值
	// 当前生命值应在对应类的成员变量里
	// 因为 DataAsset 在内存中只有一份，写在这里全地图所有的同种角色都会同时掉血
	UPROPERTY(EditAnywhere, Category = "Base Attribute")
	float MaxHealth = 100.f;


	#pragma region AI_Perception
	// 检测范围
	// 有感知才会显示此项
	UPROPERTY(EditAnywhere, Category = "Attributes|AI", meta = (EditCondition = "CharacterType == ECharacterType::Enemy && AIDetectionLevel != EAIDetectionLevel::NoPerception", EditConditionHides))
	float DetectionRange = 1200.f;

	// 视角角度
	// 有感知才会显示此项
	UPROPERTY(EditAnywhere, Category = "Attributes|AI", meta = (EditCondition = "CharacterType == ECharacterType::Enemy && AIDetectionLevel != EAIDetectionLevel::NoPerception", EditConditionHides))
	float VisionAngle = 60.f;
	#pragma endregion

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

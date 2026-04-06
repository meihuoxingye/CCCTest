// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FiringSubsystem.generated.h"


// 极其精简的射击状态数据，对 CPU 高速缓存（Cache）极其友好
USTRUCT()
struct FActiveShooterData
{
	GENERATED_BODY()

	// 谁在开火？
	// 【极其重要】必须用 TWeakObjectPtr (弱指针)！
	// 因为怪物可能随时被打死销毁，如果是普通指针，这里就会变成野指针导致游戏崩溃

	// TObjectPtr（强引用）指向并占用内存，占用的内存随所在结构体或组件一起被摧毁
	// 在内存释放前强引用一直有效，无法作为是否该销毁所在结构体的依据，从而导致内存泄漏
	// TWeakObjectPtr（弱引用），当被引用的外部对象（战斗组件）销毁时，指针就会失效
	// 因此弱引用可以作为是否该销毁所在结构体的依据，这样就不会在子系统里发生内存泄漏了
	TWeakObjectPtr<class UMyCombatComponent> CombatComp;

	// 这把枪的射击间隔，之后将会储存从外部传入的真实射击间隔数值
	float FireInterval = 0.1f;

	// 当前累加时间
	float CurrentCooldown = 0.f;
};

UCLASS()
class CCC_API UFiringSubsystem : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()
	
public:
	// 外部调用：按下左键 / AI 决定开火
	// 在外部把开火的战斗组件传入到子系统定义的结构体中，将此结构体添加到开火名单数组中
	// 开火间隔参数 Interval 也在外部传进来，而不是在内部调用其他头文件的值，优化性能
	void RegisterShooter(class UMyCombatComponent* Shooter, float Interval);

	// 外部调用：松开左键 / AI 停止开火
	// 通过比对停止开火对象的战斗组件，从开火名单数组中删去对应的结构体
	void UnregisterShooter(class UMyCombatComponent* Shooter);

	// --- FTickableGameObject 接口 ---
	virtual void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UMyCombatSubsystem, STATGROUP_Tickables); }

private:
	// 连续内存数组！CPU 遍历它的速度比遍历场景里的 Actor 快成百上千倍！
	TArray<FActiveShooterData> ActiveShooters;
};

// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon/FiringSubsystem.h"

// 自定义战斗组件
#include "Component/CombatSystem/MyCombatComponent.h"

void UFiringSubsystem::RegisterShooter(UMyCombatComponent* Shooter, float Interval)
{
	if (!Shooter) return;

	// 检查数组里的结构体的成员变量 CombatComp 是否与传入的战斗组件 Shooter 相同
	// 传入 Lambda 逻辑，遍历数组中结构体里的成员变量 CombatComp 到 Data，与 Shooter 比较
	bool bAlreadyExists = ActiveShooters.ContainsByPredicate([Shooter](const FActiveShooterData& Data) {
		return Data.CombatComp == Shooter;
		});
	// 相同则退出，不参与注册
	if (bAlreadyExists) return;

	FActiveShooterData NewShooter;
	NewShooter.CombatComp = Shooter;
	NewShooter.FireInterval = Interval;
	NewShooter.CurrentCooldown = 0.f; // 注册时立即可以开第一枪

	// 添加到数组末位
	ActiveShooters.Add(NewShooter);
}

void UFiringSubsystem::UnregisterShooter(UMyCombatComponent* Shooter)
{
	if (!Shooter) return;

	// 使用 RemoveAllSwap 实现 O(1) 级别的极速删除
	// ActiveShooters.RemoveAllSwap 会把数组里所有结构体传入 Data，而外部调用传入的 UMyCombatComponent 会传入 Shooter
	// Shooter 与 Data 传入 Lambda 逻辑，遍历数组中结构体里的成员变量 CombatComp 到 Data，与 Shooter 比较
	// 为什么不直接删，因为要删的是数组里的结构体，判断是否要删的条件是比对结构体里的成员变量
	ActiveShooters.RemoveAllSwap([Shooter](const FActiveShooterData& Data) {
		return Data.CombatComp == Shooter;
		});
}

void UFiringSubsystem::Tick(float DeltaTime)
{
	// 如果没人开火，直接跳过，0 消耗
	if (ActiveShooters.Num() == 0) return;

	// 倒序遍历（因为中途可能有人死掉被移出数组）
	for (int32 i = ActiveShooters.Num() - 1; i >= 0; --i)
	{
		FActiveShooterData& ShooterData = ActiveShooters[i];

		// 安全检查，当前结构体的战斗组件是否还存在（对象可能被击杀销毁了）
		if (!ShooterData.CombatComp.IsValid())
		{
			// 将当前结构体从列表中极速剔除，防止报错
			ActiveShooters.RemoveAtSwap(i);
			continue;
		}

		// 倒计时，每帧减去帧更新时间
		ShooterData.CurrentCooldown -= DeltaTime;

		// 冷却完毕，触发发射！
		if (ShooterData.CurrentCooldown <= 0.f)
		{
			// 执行注册到数组里的战斗组件里的真正开火逻辑
			ShooterData.CombatComp->ExecuteAttack();

			// 重置冷却时间（加上扣掉的负数时间，确保极限帧率下的射击频率精准无误）
			// 这能防止“帧间漂移”。即使某帧卡顿了，下一帧也会通过负数偏移把射速补回来
			ShooterData.CurrentCooldown += ShooterData.FireInterval;
		}
	}
}
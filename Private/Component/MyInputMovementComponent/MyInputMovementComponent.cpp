// Fill out your copyright notice in the Description page of Project Settings.


#include "Component/MyInputMovementComponent/MyInputMovementComponent.h"
// 角色
#include "GameFramework/Character.h"
// 移动组件
#include "GameFramework/CharacterMovementComponent.h"


UMyInputMovementComponent::UMyInputMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = false;


}


// Called when the game starts
void UMyInputMovementComponent::BeginPlay()
{
	Super::BeginPlay();


	
}


void UMyInputMovementComponent::HandleMoveInput(const FVector2D& InputAxisVector)
{
	ControlledPawn = Cast<APawn>(GetOwner());

	if (ControlledPawn)
	{
		//处理移动
		UpdateMovement(InputAxisVector);

		// 处理转向
		UpdateRotation(InputAxisVector);
	}
}

void UMyInputMovementComponent::UpdateMovement(const FVector2D& InputAxisVector)
{
	// 向前，世界坐标系而不是角色坐标系
	ControlledPawn->AddMovementInput(
		FVector(1, 0, 0),
		InputAxisVector.X
	);
	// 向上
	ControlledPawn->AddMovementInput(
		FVector(0, -1, 0),
		InputAxisVector.Y
	);
}

void UMyInputMovementComponent::UpdateRotation(const FVector2D& InputAxisVector)
{
	// 只有当玩家按下 A 或 D（X轴不为0）时，才改变朝向
	// IsNearlyZero 相比直接用 == 0.0f，它更安全，因为它考虑了浮点数精度误差
	if (!FMath::IsNearlyZero(InputAxisVector.X))
	{
		// 是否按了 D
		bool bCurrentlyMovingRight = (InputAxisVector.X > 0.f);

		// 如果按键与朝向不一致
		if (bCurrentlyMovingRight != bWasFacingRight)
		{
			if (!bCurrentlyMovingRight)
			{
				// 如果是向左转，我们要么去 180，要么去 -180
				// FInterpTo 会根据正负号强制决定旋转方向
				// 网格体默认 Yam 为 -90
				TargetYaw = FMath::RandBool() ? 180.f - 90.f : -180.f - 90.f;
			}
			else
			{
				// 向右转时，目标永远是 0
				TargetYaw = 0.f - 90.f;
			}

			// 按键与朝向一致
			bWasFacingRight = bCurrentlyMovingRight;

			// 如果定时器还没跑，或者已经跑完了，重新启动它
			if (!GetWorld()->GetTimerManager().IsTimerActive(RotationTimerHandle))
			{
				GetWorld()->GetTimerManager().SetTimer(
					RotationTimerHandle,
					this,
					&UMyInputMovementComponent::SmoothRotate,
					Time,
					true
				);
			}
		}
	}
}

void UMyInputMovementComponent::SmoothRotate()
{
	if (!ControlledPawn) return;

	ACharacter* Character = Cast<ACharacter>(ControlledPawn);
	if (!Character) return;

	USkeletalMeshComponent* Mesh = Character->GetMesh();
	if (!Mesh) return;


	// 获取当前的 Yaw 值（普通浮点数）
	float CurrentYaw = Mesh->GetRelativeRotation().Yaw;

	// CurrentYaw - TargetYaw（求绝对距离），例如 175 - (-180) = 355
	// FMath::UnwindDegrees(...)（寻找捷径），355 转为 -5
	// TargetYaw + ...（重新定位起点）
	float AdjustedCurrentYaw = TargetYaw + FMath::UnwindDegrees(CurrentYaw - TargetYaw);

	// 检查是否插值到位
	if (FMath::IsNearlyEqual(AdjustedCurrentYaw, TargetYaw, 0.5f))
	{
		Mesh->SetRelativeRotation(FRotator(0.f, TargetYaw, 0.f));
		GetWorld()->GetTimerManager().ClearTimer(RotationTimerHandle);
		return;
	}

	// 使用 FInterpTo 而不是 RInterpTo
	// 它会像在数轴上走一样，0 到 -180 必须经过负数，绝不会往正数绕
	// 计算步进为计算器时间
	float NewYaw = FMath::FInterpTo(AdjustedCurrentYaw, TargetYaw, Time, RotationInterpSpeed);

	// 只旋转模型组件，不触动胶囊体和根节点
	Mesh->SetRelativeRotation(FRotator(0.f, NewYaw, 0.f));
}


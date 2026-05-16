// Fill out your copyright notice in the Description page of Project Settings.


#include "Component/CombatSystem/MyCombatComponent.h"
// 基础角色类
#include "Character/BaseCharacter.h"
// 系统函数库，可用来调试打印
#include "Kismet/KismetSystemLibrary.h"
// 基础抛射物类
#include "Weapon/Projectile/MyBaseProjectile.h"
// 子弹子系统类
#include "Weapon/AsyncLineTraceBullet/MyBulletSubsystem.h"
// 武器基类
#include "Weapon/MyWeaponBase.h"
// 武器数据资产配置类
#include "Component/CombatSystem/MyWeaponDataAsset.h"
// 开火子系统类
#include "Weapon/FiringSubsystem.h"
// AI 控制器类
#include "AI/Controller/MyAIController.h"

// Sets default values for this component's properties
UMyCombatComponent::UMyCombatComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}

void UMyCombatComponent::ExecuteAttack()
{
	// 当前没有使用的武器、未设置武器数据资产配置或拥有组件者不是 Charater
	if (!CachedActiveWeapon || !CachedOwner || !CachedConfig) return;

	// 根据数据资产配置决定执行线迹追踪还是生成抛射物
	if (CachedConfig->FireType == EWeaponFireType::Hitscan)
	{
		PerformHitscan();
	}
	else
	{
		SpawnProjectile();
	}
}

bool UMyCombatComponent::SwitchToActiveWeapon(AMyWeaponBase* NewWeapon)
{
	if (!NewWeapon) return false;

	// 先尝试获取网格，如果没有网格，直接拒绝装配，并报错！
	if (!NewWeapon->GetWeaponMuzzleComponent())
	{
		UE_LOG(LogTemp, Error, TEXT("武器 [%s] 忘记配置静态网格！拒绝装配！"), *NewWeapon->GetName());
		return false;
	}

	// 先尝试获取配置，如果忘了配数据，直接拒绝装配，并报错！
	if (!NewWeapon->GetWeaponConfig())
	{
		UE_LOG(LogTemp, Error, TEXT("武器 [%s] 忘记配置 WeaponConfig 数据资产！拒绝装配！"), *NewWeapon->GetName());
		return false;
	}

	// 缓存当前使用武器
	CachedActiveWeapon = NewWeapon;

	// 缓存当前武器网格
	CachedWeaponMesh = CachedActiveWeapon->GetWeaponMuzzleComponent();

	// 缓存武器携带的数据资产配置
	CachedConfig = CachedActiveWeapon->GetWeaponConfig();

	// 缓存枪口插槽名
	CachedMuzzleSocket = CachedConfig->MuzzleSocketName;

	// 缓存子弹子系统
	CachedBulletSubsystem = GetWorld()->GetSubsystem<UMyBulletSubsystem>();

	return true;
}

void UMyCombatComponent::StartWeaponFire()
{
	if (!CachedActiveWeapon || !CachedConfig) return;

	// 获取开火子系统，把我自己注册进“射击大名单”
	if (UFiringSubsystem* CombatSubsystem = GetWorld()->GetSubsystem<UFiringSubsystem>())
	{
		CombatSubsystem->RegisterShooter(this, CachedConfig->RefireTime);
	}
}

void UMyCombatComponent::StopWeaponFire()
{
	// 告诉开火子系统，把我从大名单里划掉
	if (UFiringSubsystem* CombatSubsystem = GetWorld()->GetSubsystem<UFiringSubsystem>())
	{
		CombatSubsystem->UnregisterShooter(this);
	}
}

void UMyCombatComponent::SpawnDefaultWeapon()
{
	// 配置生成参数
	// 定义一个生成参数清单，它的大多数值都是空的，所以需要手动填上最重要的两项
	FActorSpawnParameters SpawnParams;
	// 这把枪属于谁
	SpawnParams.Owner = CachedOwner;
	// 谁发起的这次行为
	SpawnParams.Instigator = CachedOwner;

	// 生成武器实体
	AMyWeaponBase* SpawnedWeapon = GetWorld()->SpawnActor<AMyWeaponBase>(CachedOwner->GetDefaultWeaponClass(), SpawnParams);

	// 如果生成失败则退出
	if (!SpawnedWeapon) return;

	// 生成成功，实体在世界里了，但装配失败
	if (!SwitchToActiveWeapon(SpawnedWeapon))
	{
		// 必须亲手杀掉刚才生成的实体，把它从关卡里抹除
		SpawnedWeapon->Destroy();
		return;
	}

	// 检查是否忘记设置插槽名
	if (CachedConfig->WeaponSocketName.IsNone())
	{
		// 在控制台和日志中输出警告，%s 会替换为当前武器数据资产的名字
		UE_LOG(LogTemp, Warning, TEXT("武器数据资产 [%s] 忘记设置 WeaponSocketName 了！"), *CachedConfig->GetName());

		// 可选：在此处直接返回，防止子弹从角色原点发射
		return;
	}

	// 吸附到角色插槽上
	AttachWeaponToSocket(CachedActiveWeapon);
}

void UMyCombatComponent::AttachWeaponToSocket(AMyWeaponBase* SpawnedWeapon)
{
	SpawnedWeapon->AttachToComponent(
		CachedOwner->GetMesh(),
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		CachedConfig->WeaponSocketName
	);
}

// Called when the game starts
void UMyCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	// 缓存组件拥有者
	CachedOwner = Cast<ABaseCharacter>(GetOwner());

}

void UMyCombatComponent::CachedController()
{
	bControllerChecked = true;

	// 缓存组件拥有者的控制器
	CachedOwnerController = CachedOwner->GetController();

	// 尝试将组件拥有者的控制器转为玩家控制器
	CachedPlayerController = Cast<APlayerController>(CachedOwnerController);
	// 尝试将组件拥有者的控制器转为 AI 控制器
	CachedAIController = Cast<AAIController>(CachedOwnerController);

	if (CachedPlayerController)
	{
		// 如果是真人玩家，顺便把它的相机管理器也锁死缓存下来
		CachedCameraManager = CachedPlayerController->PlayerCameraManager;

		CachedAIController = nullptr;
	}
	else if (CachedAIController)
	{
		CachedPlayerController = nullptr;
		CachedCameraManager = nullptr;
	}
}


// Called every frame
void UMyCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UMyCombatComponent::PerformHitscan()
{
	// 把缓存代码放在要用之前的地方,加个全局布尔控制只运行一次，这种方法称为懒加载
	if (!bControllerChecked)
	{
		CachedController();
	}
	
	// 检查是否忘记设置插槽名
	if (CachedMuzzleSocket.IsNone())
	{
		// 在控制台和日志中输出警告，%s 会替换为当前武器数据资产的名字
		// 编译器期望接收到的是一个底层的 C 风格字符指针，所以要加*
		UE_LOG(LogTemp, Warning, TEXT("武器数据资产 [%s] 忘记设置 MuzzleSocketName 了！"), *CachedConfig->GetName());

		// 可选：在此处直接返回，防止子弹从角色原点发射
		return;
	}

	if (!CachedOwnerController) return;
	if (!(CachedPlayerController || CachedAIController)) return;

	// 一次性获取完整的 Transform（包含位置、旋转、缩放），底层骨骼树查询开销直接砍掉 2/3！
	const FTransform MuzzleTransform = CachedWeaponMesh->GetSocketTransform(CachedMuzzleSocket, RTS_World);

	// 从 Transform 中直接拆出位置
	const FVector MuzzleLoc = MuzzleTransform.GetLocation();
	// 从 Transform 中拆出欧拉角旋转（供玩家分支修改 Pitch 使用）
	FRotator MuzzleRot = MuzzleTransform.Rotator();

	FVector Dir = FVector::ZeroVector;

	if (CachedPlayerController && CachedCameraManager)
	{
		// 存放枪口在屏幕上的位置
		FVector2D MuzzleScreenPos;
		// 存放鼠标在屏幕上的位置
		FVector2D MousePosition;

		// ProjectWorldLocationToScreen：将 3D 世界中的枪口位置（MuzzleLoc）转换为屏幕上的 2D 像素坐标
		// GetMousePosition：获取当前鼠标光标在屏幕上的像素坐标
		// 只有这两个位置都成功获取到了，才执行里面的瞄准逻辑
		if (CachedPlayerController->ProjectWorldLocationToScreen(MuzzleLoc, MuzzleScreenPos) && CachedPlayerController->GetMousePosition(MousePosition.X, MousePosition.Y))
		{
			// 计算从枪口屏幕位置指向鼠标屏幕位置的 2D 向量
			FVector2D ScreenDelta = MousePosition - MuzzleScreenPos;

			// --- 严谨的透视修正系数计算 ---
			// 初始化修正系数为 1.0（即不修正）
			float CorrectionFactor = 1.0f;
			// // 确保相机管理器有效
			if (CachedPlayerController->PlayerCameraManager)
			{
				// 获取相机最终的世界旋转（即便你只改了弹簧臂，这里拿到的也是正确的）
				float CamPitch = CachedPlayerController->PlayerCameraManager->GetCameraRotation().Pitch;

				// 【严谨数学推导】：
				// 在俯视角下，屏幕 Y 轴感知的距离是被“压缩”了的。
				// 压缩比例正好是相机俯角的余弦值 (Cos)。
				// 为了还原真实的 3D 仰角，我们需要把屏幕 Y 轴的位移“拉伸”回去。
				// FMath::DegreesToRadians 转为弧度
				float CosAlpha = FMath::Cos(FMath::DegreesToRadians(CamPitch));

				// 修正系数 = 1 / Cos(相机角)
				// 防止相机完全垂直（90度）时除以0导致无穷大，设定一个安全上限
				CorrectionFactor = 1.0f / FMath::Max(0.01f, FMath::Abs(CosAlpha));
			}

			// 使用修正后的 Y 轴计算夹角
			// 使用 Atan2 计算 2D 平面上的夹角
			// 参数1 (Y)：-ScreenDelta.Y * CorrectionFactor
			//   - 取负号是因为屏幕坐标系 Y 向下为正，而数学坐标系向上为正
			//   - 乘以 CorrectionFactor 是为了抵消上面说的透视压缩
			// 参数2 (X)：FMath::Abs(ScreenDelta.X)
			//   - 取绝对值是为了让计算结果永远相对于“前方”
			//   - 这样无论你面朝左还是右，算出来的 Pitch 都是正确的抬枪角度
			float AngleRad = FMath::Atan2(-ScreenDelta.Y * CorrectionFactor, FMath::Abs(ScreenDelta.X));
			float AngleDeg = FMath::RadiansToDegrees(AngleRad);

			// 叠加原本的 Pitch
			// MuzzleRot 原本存放的是当前动画帧枪口的旋转
			// 加上 AngleDeg，意味着在动画姿势的基础上，根据鼠标位置进行上下偏移
			MuzzleRot.Pitch += AngleDeg;

			// 使用数据资产中的配置进行限幅，防止枪管翻转
			MuzzleRot.Pitch = FMath::Clamp(MuzzleRot.Pitch, CachedConfig->MinimumPitchAngle, CachedConfig->MaximumPitchAngle);
		}

		// 获取插槽旋转，然后用 Vector() 将欧拉角（旋转）转为前向向量
		Dir = MuzzleRot.Vector();
	}
	else if (CachedAIController)
	{
		Dir = CachedWeaponMesh->GetSocketRotation(CachedMuzzleSocket).Vector();
	}

	// 发射子弹
	if (CachedBulletSubsystem)
	{
		// 传参：谁开的枪，哪里开的，方向，速度，寿命
		CachedBulletSubsystem->FireBullet(CachedOwner, MuzzleLoc, Dir, CachedConfig->BulletSpeed, CachedConfig->BulletLifespan);
	}
}


// 待修改
void UMyCombatComponent::SpawnProjectile()
{
	if (!CachedConfig->ProjectileClass || !CachedWeaponMesh) return;

	// 从武器网格体插槽上获取枪口位置和旋转
	const FVector Loc = CachedWeaponMesh->GetSocketLocation(CachedConfig->MuzzleSocketName);
	const FRotator Rot = CachedWeaponMesh->GetSocketRotation(CachedConfig->MuzzleSocketName);

	FActorSpawnParameters Params;
	Params.Owner = GetOwner();
	Params.Instigator = CachedOwner;

	// 生成那个“带着原生抛射物组件”的子弹，生成后逻辑交给子弹自己
	GetWorld()->SpawnActor<AMyBaseProjectile>(CachedConfig->ProjectileClass, Loc, Rot, Params);
}
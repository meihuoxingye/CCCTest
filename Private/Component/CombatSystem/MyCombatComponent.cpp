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

// 能识别 UWorld 指针
#include "Engine/World.h"
// 使用骨骼网格体的独有函数
#include "Components/SkeletalMeshComponent.h"
// 使用静态网格体的独有函数
#include "Components/StaticMeshComponent.h"

#include "Kismet/GameplayStatics.h"
#include "Character/CharacterAttributeDataAsset.h"
#include "Engine/HitResult.h"
#include "GameFramework/DamageType.h"

// 💥【修改说明】：补充计时器系统头文件，解决 FTimerManager 不完整类型报错
#include "TimerManager.h"


// ==============================================================================
// 核心生命周期 (Core Lifecycle)
// ==============================================================================
#pragma region

// Sets default values for this component's properties
UMyCombatComponent::UMyCombatComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// 基础声明：该类组件具备同步潜质
	SetIsReplicatedByDefault(true);
}

// Called when the game starts
void UMyCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	// 缓存组件拥有者
	CachedOwner = Cast<ABaseCharacter>(GetOwner());

	// 【新增代码】：初始化血量并接入痛觉神经
	if (CachedOwner)
	{
		// 1. 一次性提取并缓存数据资产中的 MaxHealth，消除运行时指针跳转开销
		if (const UCharacterAttributeDataAsset* Config = CachedOwner->GetAttributeConfig())
		{
			CachedMaxHealth = Config->MaxHealth;
			CurrentHealth = CachedMaxHealth; // 满血出生
		}

		// 2. 将组件的受击神经，死死焊在宿主(角色)的底层受击事件上
		CachedOwner->OnTakeAnyDamage.AddDynamic(this, &UMyCombatComponent::HandleTakeDamage);
	}
}

void UMyCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 致命防御：如果组件被摧毁（跨图、死亡），必须强行掐死发车倒计时！
	// 防止定时器触发已释放的内存指针 (FlushDamageBatch) 引发核心崩溃。
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BatchTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

// Called every frame
void UMyCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

#pragma endregion


// ==============================================================================
// 武器生成与装配 (Weapon Spawn & Assembly)
// ==============================================================================
#pragma region

void UMyCombatComponent::SpawnDefaultWeapon()
{
	// 【新增代码】：极限防御，防止组件被挂载在错误的目标上导致崩溃
	if (!CachedOwner)
	{
		UE_LOG(LogTemp, Error, TEXT("MyCombatComponent 只能挂载在 ABaseCharacter 及其子类上！"));
		return;
	}

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

#pragma endregion


// ==============================================================================
// 战斗指令与状态 (Combat Commands & State)
// ==============================================================================
#pragma region

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

void UMyCombatComponent::ProcessBulletHit(const FHitResult& Hit)
{
	// 防御校验，确保打中了东西，且数据资产还在
	if (!Hit.GetActor() || !CachedConfig) return;

	// 💥【修改说明】：第二条轨的“本地防火墙”。如果是多播在队友屏幕上生成的子弹撞到了怪，立刻掐断！
	// 只有控制这个角色的本地玩家本机，才有资格下发真实的物理扣血指令，彻底杜绝一枪多倍伤害。
	if (CachedOwner && !CachedOwner->IsLocallyControlled()) return;

	// 💥【修改说明】：不立刻发 RPC，而是将目标压入打包缓冲池，使用 AddUnique 防止散弹枪同一帧多次命中同一部位重复入栈
	BatchedTargets.AddUnique(Hit.GetActor());

	// 💥【修改说明】：微型窗口聚合机制。
	// 如果这是缓冲池里的第一颗子弹（计时器未激活），则立刻启动 0.05 秒（50毫秒）的发车倒计时。
	// 这 50ms 内后续所有的命中，只会被压入数组，不会触发新的倒计时。
	if (!GetWorld()->GetTimerManager().IsTimerActive(BatchTimerHandle))
	{
		GetWorld()->GetTimerManager().SetTimer(BatchTimerHandle, this, &UMyCombatComponent::FlushDamageBatch, 0.05f, false);
	}
}

void UMyCombatComponent::HandleTakeDamage(AActor* DamagedActor, float Damage, const class UDamageType* DamageType, class AController* InstigatedBy, AActor* DamageCauser)
{
	if (Damage <= 0.f || CurrentHealth <= 0.f) return;

	// 真实扣血，使用预缓存的 CachedMaxHealth 做界限钳制
	CurrentHealth = FMath::Clamp(CurrentHealth - Damage, 0.f, CachedMaxHealth);

	// 立刻拉响原生广播！通知挂载此频道的 UI 刷新血条
	OnHealthChangedNative.Broadcast(CurrentHealth, CachedMaxHealth);

	if (CurrentHealth > 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] 挨打了！掉了 %f 血，还剩 %f，正在播放受击僵直！"), *DamagedActor->GetName(), Damage, CurrentHealth);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] 寄了！"), *DamagedActor->GetName());
	}
}

#pragma endregion


// ==============================================================================
// 底层开火系统实现 (Low-Level Firing Implementation)
// ==============================================================================
#pragma region

void UMyCombatComponent::PerformHitscan()
{
	// 检测当前控制权是否发生了变更（完美兼容换人系统），若变更了就重新缓存
	// 把缓存代码放在要用之前的地方,加个条件判断只运行一次，这种方法称为懒加载
	if (CachedOwner && CachedOwner->GetController() != CachedOwnerController)
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

	// 💥【修改说明】：5.8 内部优化路径，如果不需要缩放，直接拿位姿更省性能
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
				// 💥【修改说明】：采纳稳定性建议，使用 Clamp 将底层余弦值限制在 0.05 ~ 1.0 之间
				// 完美规避极限垂直视角（接近90度）下的除零或浮点数爆炸危险
				CorrectionFactor = 1.0f / FMath::Clamp(FMath::Abs(CosAlpha), 0.05f, 1.0f);
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

		// 💥【修改说明】：第一条轨启动。同一帧发射 Server RPC 向服务器上报动作请求，触发全网多播
		Server_PlayFireAction(MuzzleLoc, Dir);
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

#pragma endregion


// ==============================================================================
// 辅助与缓存工具 (Utilities & Caching)
// ==============================================================================
#pragma region

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

#pragma endregion


// ==============================================================================
// 联机底层探针 (Network Probes)
// ==============================================================================
#pragma region

void UMyCombatComponent::Server_PlayFireAction_Implementation(FVector MuzzleLoc, FVector FireDirection)
{
	// 💥【修改说明】：收到开火上报，服务器不做任何校验阻滞，当一个无情的信号塔，向全网所有副机广播该视觉动作
	Multicast_PlayFireAction(MuzzleLoc, FireDirection);
}

void UMyCombatComponent::Multicast_PlayFireAction_Implementation(FVector MuzzleLoc, FVector FireDirection)
{
	// 💥【修改说明】：视觉防重影锁。如果是开枪的玩家本人，直接驳回（因为他已经在 PerformHitscan 里播过本地特效了）
	if (CachedOwner && CachedOwner->IsLocallyControlled()) return;

	// 💥【修改说明】：队友屏幕上的视觉同步。发射出一颗一模一样的子弹（轨迹、速度完全一致）
	// 当这颗子弹撞墙或撞怪时，也会进入 ProcessBulletHit 流程，但会被那里的本地判定锁拦截，避免对怪物造成二次虚假伤害！
	if (CachedBulletSubsystem && CachedConfig)
	{
		CachedBulletSubsystem->FireBullet(CachedOwner, MuzzleLoc, FireDirection, CachedConfig->BulletSpeed, CachedConfig->BulletLifespan);
	}
}

void UMyCombatComponent::Server_ApplyBatchedDamage_Implementation(const TArray<AActor*>& TargetEnemies)
{
	// 💥【修改说明】：服务器权威执行伤害。不信任客户端传来的数值，直接从配置表强读 CachedConfig->Damage
	if (!CachedOwner || !CachedConfig) return;

	// 缓存数据，避免在循环中反复跳转指针查表
	const float ActualDamage = CachedConfig->Damage;
	AController* InstigatorCtrl = CachedOwner->GetController(); // 动态获取最新控制器避免缓存脱节

	// 💥【修改说明】：计算物理射程极限（子弹速度 * 寿命）。追加 500.f 的冗余量以容忍客户端与服务器之间的网络漂移
	const float MaxRangeSq = FMath::Square(CachedConfig->BulletSpeed * CachedConfig->BulletLifespan + 500.f);
	const FVector ShooterLoc = CachedOwner->GetActorLocation();

	// 拆包并逐个执行权威扣血
	for (AActor* Target : TargetEnemies)
	{
		// 💥【修改说明】：严密防御。使用 IsValid 替代指针判空，防止目标正在处于 PendingKill（即将被回收）状态
		if (IsValid(Target))
		{
			// 💥【修改说明】：防全图秒杀底线校验。判断距离是否超出物理极限，如果是，直接过滤该非法伤害请求
			float DistSq = FVector::DistSquared(ShooterLoc, Target->GetActorLocation());
			if (DistSq <= MaxRangeSq)
			{
				// 核心解耦：使用 UE 全局伤害总线，替代繁琐的 Cast<ABaseCharacter> 类型判断。
				// 1. 泛用性：无视目标类型（角色、木箱或油桶），只要对方绑定了 OnTakeAnyDamage 委托就能接收到伤害。
				// 2. AI 联动：该接口会自动把开火者（CachedOwnerController）的信息传给虚幻底层的 AI 感知系统。
				// 3. 仇恨机制：这能让受击的 AI 明确知道“是谁打了我”，从而执行正确的转身或追击逻辑。
				UGameplayStatics::ApplyDamage(
					Target,
					ActualDamage,
					InstigatorCtrl,
					CachedOwner,
					UDamageType::StaticClass()
				);
			}
		}
	}
}

void UMyCombatComponent::FlushDamageBatch()
{
	// 准备一个干净的原始指针数组用于网络传输
	TArray<AActor*> ValidTargets;

	for (const TWeakObjectPtr<AActor>& WeakTarget : BatchedTargets)
	{
		// 严密防御：只有在这 50ms 内依然存活（没被别人打死或被 GC）的合法活体目标，才有资格结算伤害
		if (WeakTarget.IsValid())
		{
			ValidTargets.Add(WeakTarget.Get());
		}
	}

	// 只要存在有效目标，一脚油门发往服务器
	if (ValidTargets.Num() > 0)
	{
		Server_ApplyBatchedDamage(ValidTargets);
	}

	// 清空缓冲池，等待下一波扣动扳机
	BatchedTargets.Empty();
}

#pragma endregion
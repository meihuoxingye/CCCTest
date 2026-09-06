// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MyCombatComponent.generated.h"


// ------------------------------------------------------------------------------
// 血量变化广播总线 (事件驱动)
// 它相当于一个广播喇叭。当角色血量变化时，不用去“寻找”UI更新，而是直接对着全网喊一嗓子，实现代码完全解耦。
//  - 发布者：UMyCombatComponent (在 TakeDamage 扣完血后，调用 Broadcast 发送广播)
//  - 订阅者：UMyCharacterStatusWidget (在初始化时，调用 AddUObject 给自己戴上耳机监听)
//  - Param 1 (float): 挨打扣完之后的最新当前血量。
//  - Param 2 (float): 从数据资产里缓存的最大血量上限。
// 为什么要用“原生”多播 (去掉了 DYNAMIC 宏)：
//  - 带 DYNAMIC 的委托是为了能在蓝图节点里连线，会强制经过虚幻极慢的“反射系统”。
//  - 我们现在的 UI 绑定全在 C++ 里写死了，不需要给蓝图用，所以直接用纯 C++ 函数指针直接调用（Native），性能最快。
// ------------------------------------------------------------------------------
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnHealthChangedNative, float /*NewCurrentHealth*/, float /*MaxHealth*/);


UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class CCC_API UMyCombatComponent : public UActorComponent
{
	GENERATED_BODY()


	// ==============================================================================
	// 核心生命周期 (Core Lifecycle)
	// ==============================================================================
public:
	// Sets default values for this component's properties
	UMyCombatComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	// 💥【修改说明】：重写 EndPlay，负责组件销毁时的内存级清理，斩断定时器野指针
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 💥【修改说明】：补全组件的网络生命周期，用于注册需要跨网同步的变量
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;


	// ==============================================================================
	// 武器生成与装配 (Weapon Spawn & Assembly)
	// ==============================================================================
public:
	// 为拥有该组件的角色生成默认武器
	// 由基础角色调用
	void SpawnDefaultWeapon();
	// 将生成的默认武器吸附到角色的骨骼插槽上
	void AttachWeaponToSocket(class AMyWeaponBase* SpawnedWeapon);

private:
	// 将 NewWeapon 切换为当前使用武器，并获取武器的网格与数据资产配置的接口，然后缓存它们
	// 设置为布尔值，缓存失败返回 false，用于在其他函数里进行条件判断
	bool SwitchToActiveWeapon(class AMyWeaponBase* NewWeapon);


	// ==============================================================================
	// 战斗指令与状态 (Combat Commands & State)
	// ==============================================================================
public:
	// 外部调用，执行开火逻辑
	void ExecuteAttack();
	// 外部调用，将组件注册进射击子系统的开火冷却名单数组中
	void StartWeaponFire();
	// 外部调用，将组件从射击子系统的开火冷却名单数组中移除
	void StopWeaponFire();

	// 供 MyBulletSubsystem 射线打中目标后回调的命中处理函数
	// 💥【修改说明】：接收子弹击中时传回的“该子弹开火瞬间绑定的数据资产”，解决换枪导致的配置劫持，且利于后续扩展其他属性读取。
	void ProcessBulletHit(const struct FHitResult& Hit, const class UMyWeaponDataAsset* HitWeaponConfig);

	// 给外界（UI）开放一个获取当前血量的接口
	FORCEINLINE float GetCurrentHealth() const { return CurrentHealth; }

	// 开放给 UI 绑定的纯 C++ 极速广播频道
	FOnHealthChangedNative OnHealthChangedNative;

private:
	// 真实的活体当前血量
	// 在 C++ 层面绝对私有，但在蓝图层面允许读取，只需加上 meta = (AllowPrivateAccess = "true")
	// 💥【修改说明】：添加 ReplicatedUsing。当服务器的血量同步到客户端时，底层会自动唤醒 OnRep_CurrentHealth！
	UPROPERTY(ReplicatedUsing = OnRep_CurrentHealth, VisibleAnywhere, BlueprintReadOnly, Category = "Attributes", meta = (AllowPrivateAccess = "true"))
	float CurrentHealth;

	// 缓存的最大血量，杜绝受伤时高频访问 DataAsset 指针
	float CachedMaxHealth = 100.f;

	// 挂载到原生底层受击委托 OnTakeAnyDamage 的回调函数
	UFUNCTION()
	void HandleTakeDamage(AActor* DamagedActor, float Damage, const class UDamageType* DamageType, class AController* InstigatedBy, AActor* DamageCauser);

	// 💥【修改说明】：网络同步回调函数，当客机收到新血量时自动执行。
	// 虚幻的神仙机制：给 OnRep 函数加上一个同类型的参数 (OldHealth)，引擎底层就会自动把修改前的“旧血量”塞进来，方便我们算差值！
	UFUNCTION()
	void OnRep_CurrentHealth(float OldHealth);


	// ==============================================================================
	// 底层开火系统实现 (Low-Level Firing Implementation)
	// ==============================================================================
private:
	// 执行射线检测
	void PerformHitscan();
	// 召唤抛射物实体
	void SpawnProjectile();


	// ==============================================================================
	// 辅助与缓存工具 (Utilities & Caching)
	// ==============================================================================
protected:
	// 缓存组件拥有者的玩家或 AI 控制器；
	// 组件的 BeginPlay 执行时，角色可能还没有被控制器控制，所以不直接在 BeginPlay 里缓存
	void CachedController();

private:
	// 缓存组件拥有者的指针
	UPROPERTY()
	TObjectPtr<class ABaseCharacter> CachedOwner;

	// 缓存组件拥有者的控制器的指针
	UPROPERTY()
	TObjectPtr<class AController> CachedOwnerController;

	// 缓存当前使用的武器
	UPROPERTY()
	TObjectPtr<class AMyWeaponBase> CachedActiveWeapon;

	// 缓存当前使用的武器的数据资产配置
	// 获取数据资产配置的 GetWeaponConfig 被 const 保护，这里也要加 const
	// 指针+const，保护的是指针指向的数据，不影响设置指针指向哪
	UPROPERTY()
	TObjectPtr<const class UMyWeaponDataAsset> CachedConfig;

	// 缓存当前使用的武器的网格
	// 静态网格也能使用插槽，且性能更好
	UPROPERTY()
	TObjectPtr<const class UStaticMeshComponent> CachedWeaponMesh;

	// 缓存当前使用的武器的枪口插槽
	FName CachedMuzzleSocket;

	// 缓存子弹子系统指针
	UPROPERTY()
	TObjectPtr<class UMyBulletSubsystem> CachedBulletSubsystem;

	// 记录是否已经尝试去抓过控制器了
	bool bControllerChecked = false;

	// 缓存玩家控制器（如果是 AI，则自动为 nullptr）
	UPROPERTY()
	TObjectPtr<class APlayerController> CachedPlayerController;

	// 缓存玩家相机管理器（如果是 AI，则自动为 nullptr）
	UPROPERTY()
	TObjectPtr<class APlayerCameraManager> CachedCameraManager;

	// 缓存 AI 控制器（如果是 玩家，则自动为 nullptr）
	UPROPERTY()
	TObjectPtr<class AAIController> CachedAIController;


	// ==============================================================================
	// 联机底层探针 (Network Probes)
	// ==============================================================================
public:
	// 轨道一（上半截：向总台上报）
	// 客机无权直接广播。本地开火时，向服务器发射 Server RPC 请求代为广播。
	// 极高频瞬态事件，无需可靠传输，丢失即弃绝不阻塞带宽。
	UFUNCTION(Server, Unreliable)
	void Server_PlayFireAction(FVector MuzzleLoc, FVector FireDirection);

	// 轨道一（下半截：向全网下发）
	// 服务器收到上报后，充当信号塔，向全网所有客机下发 Multicast RPC 视觉动作。
	// 配合 cpp 里的 IsLocallyControlled() 拦截，实现队友看特效，自己不重复看。
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayFireAction(FVector MuzzleLoc, FVector FireDirection);

	// 轨道二：批量伤害裁决 RPC (数值层)
	// 统一走数组通道。单发武器数组 Size 为 1，高射速或散弹枪 Size 为 N。完美适配所有枪械。
	// 💥【修改说明】：追加 HitWeaponConfig 参数，服务器直接从这个被固化的历史数据资产中读取伤害等结算数值。
	UFUNCTION(Server, Reliable)
	void Server_ApplyBatchedDamage(const TArray<AActor*>& TargetEnemies, const class UMyWeaponDataAsset* HitWeaponConfig);

private:
	// 伤害批量打包缓冲池。
	// 必须使用弱指针 (TWeakObjectPtr)，防止怪物在 50ms 的缓冲期内因其他原因死亡，导致发送野指针崩溃。
	TArray<TWeakObjectPtr<AActor>> BatchedTargets;

	// 💥【修改说明】：用于在 50ms 缓冲期内，暂存当前批次子弹所对应的数据资产指针，等待发往服务器。
	UPROPERTY()
	TObjectPtr<const class UMyWeaponDataAsset> PendingBatchConfig;

	// 打包发送发车计时器
	FTimerHandle BatchTimerHandle;

	// 清空缓冲池并向服务器发射 RPC 的收尾函数，用于在 50ms 缓冲期结束后，将池子里的目标打包发往服务器
	void FlushDamageBatch();
};
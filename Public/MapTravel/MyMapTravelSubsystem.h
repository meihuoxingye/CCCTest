// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MyMapTravelSubsystem.generated.h"

class UDataLayerAsset;
class UTeleportRoute;

// ==============================================================================
// 对点传送底层数据结构 (Point-to-Point Data Structure)
// ==============================================================================
USTRUCT(BlueprintType)
struct FDestinationRegistrationInfo
{
	GENERATED_BODY()
public:

	// 注册源实体弱引用：绑定该目标点的具体 Actor，使用弱指针防止阻碍垃圾回收
	UPROPERTY()
	TWeakObjectPtr<AActor> RegistrySource = nullptr;

	// 绝对物理变换矩阵：记录落地接机点在世界中的绝对空间坐标与旋转
	UPROPERTY()
	FTransform TargetTransform;

	// 缓存的目标数据层
	UPROPERTY()
	UDataLayerAsset* BoundDataLayer = nullptr;
};

/**
 * 负责 2.5D 横版关卡的无缝流转
 * 专职统筹物理坐标转移、时序挂起、以及服务器端的网络排队霸权
 */
UCLASS()
class CCC_API UMyMapTravelSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

	// ==============================================================================
	// 生命周期与初始化 (Lifecycle & Initialization)
	// ==============================================================================
public:

	// 覆写子系统原生初始化函数，在世界创建且子系统被拉起时执行
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// 覆写子系统原生反初始化函数，在世界销毁或子系统卸载时清理内存
	virtual void Deinitialize() override;

	// 覆写世界准备就绪钩子，在新地图加载完毕后的第一帧触发
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;


	// ==============================================================================
	// 内存缓存与数据字典 (Cache & Registry)
	// ==============================================================================
public:

	// 【新增性能优化】：O(1) 射线探测黑名单缓存，彻底告别极其耗时的 TActorIterator
	// 请在 ATopCharacter 的 BeginPlay 中调用 Register，EndPlay 中调用 Unregister
	UPROPERTY(Transient)
	TSet<TWeakObjectPtr<class ATopCharacter>> GlobalCharacterCache;

	// 将角色真身注入 O(1) 全局黑名单缓存。
	// 在 ATopCharacter::BeginPlay() 中触发。供物理折叠管线极速构建射线忽略名单，彻底消灭全图 TActorIterator 遍历带来的帧毛刺 (CPU Hitches)。
	UFUNCTION(BlueprintCallable, Category = "MapTravel|Cache")
	void RegisterCharacterToCache(class ATopCharacter* Character);

	// 将角色实体从全局黑名单缓存中除名。
	// 必须在 ATopCharacter::EndPlay() 中触发。彻底防止弱指针 Set 随游戏时长发生内存膨胀，确保服务器物理筛选的绝对高效与纯净。
	UFUNCTION(BlueprintCallable, Category = "MapTravel|Cache")
	void UnregisterCharacterFromCache(class ATopCharacter* Character);

	// 注册接机点到高速字典，绑定目标物理坐标与所属流送数据层
	UFUNCTION(BlueprintCallable, Category = "MapTravel|Registry")
	void RegisterSameMapDestination(UTeleportRoute* Route, AActor* DestinationActor, const FTransform& TargetTransform, UDataLayerAsset* BoundDataLayer = nullptr);

	// 安全注销接口：目标点被物理销毁时调用，从内存字典中清洗自身，防野指针崩溃
	UFUNCTION(BlueprintCallable, Category = "MapTravel|Registry")
	void UnregisterSameMapDestination(UTeleportRoute* Route);


	// ==============================================================================
	// 传送网关入口 (Universal Travel Gateway)
	// ==============================================================================
public:

	// 大一统传送门户：由传送门调用，系统自动判别走同地图极速瞬移还是跨地图无缝流送
	UFUNCTION(BlueprintCallable, Category = "MapTravel|Gateway")
	void ExecuteUniversalTravel(AActor* TeleportingActor, UTeleportRoute* TargetRoute);

	// 【核心功能】：大一统漫游管线的终极总闸，全自动统筹物理点穴、黑幕遮罩、时序挂起及跨界跳跃。
	// 单机/常规跨图：强制走无缝漫游 (Seamless Travel，bAbsolute = false)
	UFUNCTION(BlueprintCallable, Category = "MapTravel|Gateway")
	void ExecuteMapTravel(FName TargetLevelName);

	// 【联机专线】：房主专属建房跳转（带队发车）
	// 强制剥夺旧世界的无缝漫游面具，使用绝对跳转建立 Listen Server，并附加 ?listen 监听参数
	UFUNCTION(BlueprintCallable, Category = "MapTravel|Gateway")
	void ExecuteHostTravel(FName TargetLevelName);

	// 【联机专线】：客户端专属飞线加入
	// 绕过所有查表机制，拿着纯净的 P2P 隧道地址强制使用 ClientTravel 飞线接入房主主机
	UFUNCTION(BlueprintCallable, Category = "MapTravel|Gateway")
	void ExecuteClientJoin(const FString& ConnectString);

	// 全局解穴接口：跨地图与同地图漫游的终极收尾，恢复玩家输入与物理碰撞
	UFUNCTION(BlueprintCallable, Category = "MapTravel|Gateway")
	void RestorePlayerInput();


	// ==============================================================================
	// 服务器物理霸权 (Server-Side Physical Authority)
	// ==============================================================================
public:

	// 同地图专属统筹（服务器端霸权）：
	// 接管同图时空流速，执行绝对物理点穴，随后触发数据层预热、发送 RPC 握手与超时防死锁的高频轮询
	void ExecuteSameMapTravel(AActor* TeleportingActor, UTeleportRoute* TargetRoute);

	// 同图斩杀接口（服务器执行）：趁屏幕纯黑时，强行物理卸载老区域并激活新区域
	UFUNCTION(BlueprintCallable, Category = "MapTravel|Authority")
	void CommitSameMapDataLayer();

	// 跨图单人折叠接口（服务器执行）：专职负责在绝对黑幕下执行真身物理空间折叠
	UFUNCTION(BlueprintCallable, Category = "MapTravel|Authority")
	void SnapPlayerToDestination();

private:

	// 跨图全队阵型部署（服务器端绝对权威）：
	// 上帝视角强制接管所有 PlayerStart 与 AI，确立物理排队霸权，从根本上消灭客机 300cm 随机弹射漂移
	void DeploySquadTeammates(UWorld* World, class AMyUniversalDestination* Dest, class UMyGameInstance* GI);


	// ==============================================================================
	// 本地表现与剥离 (Local Snapping & Client Purge)
	// ==============================================================================
private:

	// 跨图传送落地物理对齐：
	// 仅允许服务器/主机真身执行探针与落地；客机必须剥离物理控制权，静默等待服务器坐标同步，严防穿模弹射
	void HandleLocalPlayerSnapping(UWorld* World, class AMyUniversalDestination* Dest);


	// ==============================================================================
	// 内部路由与状态锁 (Internal Routing & State Locks)
	// ==============================================================================
private:

	// 内部大一统收束管线：抹平差异，统一下达时序锁定与底层跳转指令
	void InternalExecuteTravel(FName TargetLevelName, const FString& TravelURL, bool bIsAbsolute, bool bIsClientJoin);

	// 转场状态互斥锁，防止玩家在转场期间重复触发导致时序错乱
	UPROPERTY()
	bool bIsTraveling = false;

	// 缓存同地图传送的目标数据层，等待黑幕时机跨系统交给流送子系统进行斩杀替换
	UPROPERTY()
	TObjectPtr<UDataLayerAsset> PendingSameMapDataLayer = nullptr;

	// 高速本地路由字典：以路由资产为 Key，缓存当前大世界内存中所有已就绪的接机点注册信息
	UPROPERTY(Transient)
	TMap<UTeleportRoute*, FDestinationRegistrationInfo> SameMapDestinationRegistry;

	// 同步握手轮询定时器句柄，用于在服务器端死等所有客机的 UI 状态机 Ready
	UPROPERTY()
	FTimerHandle SyncWaitTimerHandle;
};
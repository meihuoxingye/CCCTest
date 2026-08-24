// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
// 【核心修改】：引入降级后的传送组件，取代原来的 PlayerState 实体
#include "PlayerState/Component/TravelAndStreaming/MyMapTravelStateComponent.h"
#include "MyMapTravelSubsystem.generated.h"

class UDataLayerAsset;
class UTeleportRoute;
class UMyMapTravelStateComponent; // 【核心修改】：前向声明真理载体改为组件

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
	UPROPERTY(Transient)
	TSet<TWeakObjectPtr<class ATopCharacter>> GlobalCharacterCache;

	UFUNCTION(BlueprintCallable, Category = "MapTravel|Cache")
	void RegisterCharacterToCache(class ATopCharacter* Character);

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

	// 跨系统握手枢纽：由大管家在熄屏 UI 入场动画播完（屏幕彻底黑透）时回调。
	// 负责向服务器发射 Server_AckScreenOffReady 握手信号，确认本地视觉已完全被黑幕遮断。
	// 服务器在收到此 Ack 后，才会安全地执行玩家实体的物理坐标折叠，彻底杜绝瞬移穿帮。
	UFUNCTION(BlueprintCallable, Category = "MapTravel|Gateway")
	void NotifyLocalScreenOffFinished();

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
	// 令牌响应管线 (Token Response Pipeline)
	// ==============================================================================
public:
	// 【核心新增】：自动化契约响应！
	// 客户端 UI 的拉起、退场，物理控制权的剥夺与恢复，现在全部基于这个回调自动执行，绝对封杀越权！
	// 引入 RetryCount 熔断参数，防止网络极端延迟导致的无限递归死锁
	void HandleDeploymentTokenUpdate(UMyMapTravelStateComponent* PS, ETravelDeploymentStatus OldStatus, int32 RetryCount = 0);


	// ==============================================================================
	// 服务器物理霸权 (Server-Side Physical Authority)
	// ==============================================================================
public:

	// 同地图专属统筹（服务器端霸权）：
	// 接管同图时空流速，执行绝对物理点穴，随后触发数据层预热并下发令牌
	void ExecuteSameMapTravel(AActor* TeleportingActor, UTeleportRoute* TargetRoute);

	// 💥【核心新增】：同图跨图大一统！由 WaitingForShell 令牌触发的终极物理流送总闸
	void CheckAndExecutePhysicalDeployment();

	// 💥【保留并更新调用时机】：同图斩杀接口（服务器执行）。
	// 现已移交至 WaitingForShell 物理总闸 (SnapPlayerToDestination) 内部统一调用！
	// 趁全服玩家闭眼/UI掩护就绪时，强行物理卸载老区域并激活新区域
	UFUNCTION(BlueprintCallable, Category = "MapTravel|Authority")
	void CommitSameMapDataLayer();

	// 跨图/同图大一统折叠接口（服务器执行）：专职负责在绝对黑幕/UI掩护下执行真身物理空间折叠
	// 内部已融合统一的数据层流送(DataLayer)与肉体排布
	UFUNCTION(BlueprintCallable, Category = "MapTravel|Authority")
	void SnapPlayerToDestination();

private:

	// 跨图全队阵型部署（服务器端绝对权威）：
	// 上帝视角强制接管所有 PlayerStart 与 AI，确立物理排队霸权，从根本上消灭客机 300cm 随机弹射漂移
	void DeploySquadTeammates(UWorld* World, class AMyUniversalDestination* Dest, class UMyGameInstance* GI);


	// ==============================================================================
	// 内部路由与状态锁 (Internal Routing & State Locks)
	// ==============================================================================
private:

	// ==============================================================================
	// 内部大一统收束管线：起飞前统筹准备与时序死等。
	// 
	// 👑 主机/单机：冻结全服物理 -> 打包真身与AI躯壳 -> 下发令牌令全服黑屏 -> 死等黑屏闭合 -> 执行 ServerTravel (引擎底层自动拽走所有副机)。
	// 🚪 局外副机 (大厅飞线)：冻结本地物理 -> 拉起本地黑屏 -> 死等黑屏闭合 -> 执行 ClientTravel (飞线连入主机)。
	// ⚠️ 局内副机 (跟随跨图)：绝不执行此函数！纯被动接收令牌闭眼，靠底层的 ServerTravel 硬拽跨界。
	// ==============================================================================
	// @param TargetLevelName 目标关卡短名（查转场配置用；飞线时可空）
	// @param TravelURL       网络跳转绝对路径（IP/Session/关卡路径）
	// @param bIsAbsolute     true: 房主建房(带?listen并关无缝) / false: 无缝漫游
	// @param bIsClientJoin   true: 分流执行 ClientTravel / false: 分流执行 ServerTravel
	void InternalExecuteTravel(FName TargetLevelName, const FString& TravelURL, bool bIsAbsolute, bool bIsClientJoin);

	// 🚪 【副机专属】：客机从大厅飞线连入主机的绝对物理执行点
	void TriggerClientTravelCommand(const FString& TravelURL);

	// 👑 【主机专属】：房主带队跨图/建房的绝对物理执行点 (底层会自动硬拽副机)
	void TriggerServerTravelCommand(const FString& TravelURL, bool bIsAbsolute);


	// 转场状态互斥锁，防止玩家在转场期间重复触发导致时序错乱
	UPROPERTY()
	bool bIsTraveling = false;

	// 💥【核心内存锚点】：保护跨界 URL 防 GC 垃圾回收踩成乱码
	UPROPERTY(Transient)
	FString PendingTravelURL;

	// 【核心新增】：物理地基就绪锁 (仅服务器维护)。如果没就绪，任何副机试图连入都必须强制挂起！
	bool bIsPhysicalLayoutReady = false;

	// 💥【保留并更新调用时机】：缓存同地图传送的目标数据层
	// 等待 WaitingForShell 大一统时机，交由流送子系统进行斩杀替换
	UPROPERTY()
	TObjectPtr<UDataLayerAsset> PendingSameMapDataLayer = nullptr;

	// 高速本地路由字典：以路由资产为 Key，缓存当前大世界内存中所有已就绪的接机点注册信息
	UPROPERTY(Transient)
	TMap<UTeleportRoute*, FDestinationRegistrationInfo> SameMapDestinationRegistry;

	// 💥【更新注释】：物理轮询定时器句柄。
	// 现仅用于跨图漫游起航前的纯物理倒计时死等，时间一到即执行跨界。
	UPROPERTY()
	FTimerHandle SyncWaitTimerHandle;

	// ==============================================================================
	// 玩家实体接管与落地部署导演系统 (Deployment Director System)
	// ==============================================================================
public:
	// 由 GameMode 在 InitGame 中单向触发的专属开荒配置代码
	void ExecuteInitialBootSetup();

	// 由 GameMode 在 RestartPlayer 中调用的最终决策树 (包含基于令牌的时序挂起保护)
	bool ExecuteDeploymentDirector(class AController* NewPlayer, class AMyGameModeBase* GameMode, float CurrentWaitTime = 0.0f);

private:
	// 内部状态锁，记录 GameMode 传达的开荒旨意
	bool bIsCurrentMapInitialBoot = false;

	// 【副机专属管线】：无决策权，被动接受服务器强行分配的大名单躯壳。
	bool ExecuteGuestDeployment(class AController* NewPlayer, class AMyGameModeBase* GameMode);

	// 【主机专属管线 A】：这是开荒！执行夺舍原生假人，不排队。
	bool ExecutePioneeringPossession(class AController* NewPlayer, class AMyGameModeBase* GameMode);

	// 【主机专属管线 B】：动态图或已过开荒期。执行图纸读取，传送门动态捏人+排队。
	bool ExecuteDynamicSquadArrival(class AController* NewPlayer, class AMyGameModeBase* GameMode);

	// 内部辅助：寻找地图原生件 (主机开荒专用)
	class ATopCharacter* FindInitialStartupShell(UWorld* World);

	// 内部辅助：寻找已经由房主跨图同步过来的专属队友躯壳 (副机接管专用)
	class ATopCharacter* FindSquadTeammateShell(UWorld* World, class AMyGameModeBase* GameMode);

	// ==============================================================================
	// 核心物理与寻址大一统管线 (Unified Physics & Routing Pipeline)
	// ==============================================================================
private:

	// 统一寻址与兜底接口：
	// 严格遵循 1.首选目标点 -> 2.当前关联实体 -> 3.默认出生点 的绝对法则。
	// 强制剥离所有毒瘤缩放 (Scale)，只返回绝对纯净的 1:1:1 坐标与旋转矩阵。
	FTransform ResolveSafeDeploymentTransform(UWorld* World, const FTransform* PrimaryTransform, AActor* FallbackEntity);

	// 统一排兵布阵接口：
	// 彻底接管同图与跨图小队的 2.5D 列队、射线防穿模贴地计算，以及降临前的物理点穴。
	// 消灭原来两处高达 70 行的复制粘贴冗余。
	void ExecuteSquadFormationDeployment(UWorld* World, const FTransform& BaseTransform);
};
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MyMapTravelSubsystem.generated.h"

class UDataLayerAsset;
class UDataLayerManager;
class UTeleportRoute;

// ==============================================================================
// 关卡双轨数据结构 (Dual-Track Zone Structure)
// ==============================================================================
USTRUCT(BlueprintType)
struct FZoneDataLayerPair
{
	GENERATED_BODY()

public:
	// 反射标记：允许在蓝图和编辑器细节面板中任意读写与编辑
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MapTravel")
	// 静态艺术层指针：负责静态网格体、地形、灯光等纯视觉资产的流送
	UDataLayerAsset* ArtLayer = nullptr;

	// 反射标记：允许在蓝图和编辑器细节面板中任意读写与编辑
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MapTravel")
	// 动态玩法层指针：负责敌人生成器、关卡触发器、动态物理物件等逻辑资产的流送
	UDataLayerAsset* GameplayLayer = nullptr;
};

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

	// 【新增】：缓存的目标数据层
	UPROPERTY()
	UDataLayerAsset* BoundDataLayer = nullptr;
};

/**
 * 负责 2.5D 横版关卡的无缝流转
 * 统筹 DataLayer 预热，深度集成 LSP 流转与独立加载蒙版
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
	// 大一统传送路由中心 (Universal Routing Hub)
	// ==============================================================================
public:

	// 【修改】：参数增加 BoundDataLayer
	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void RegisterSameMapDestination(UTeleportRoute* Route, AActor* DestinationActor, const FTransform& TargetTransform, UDataLayerAsset* BoundDataLayer = nullptr);

	// 安全注销接口：目标点被物理销毁时调用，从内存字典中清洗自身，防野指针崩溃
	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void UnregisterSameMapDestination(UTeleportRoute* Route);

	// 大一统传送门户：由传送门调用，系统自动判别走同地图极速瞬移还是跨地图无缝流送
	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void ExecuteUniversalTravel(AActor* TeleportingActor, UTeleportRoute* TargetRoute);

	// 【新增】：专门供 GameInstance 瞬移落地的同一物理帧调用的斩杀接口
	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void CommitSameMapDataLayer();

	// 同地图专属执行实体：接管同图时空流速，执行绝对物理点穴，随后将坐标抛给大管家折叠
	void ExecuteSameMapTravel(AActor* TeleportingActor, UTeleportRoute* TargetRoute);


	// ==============================================================================
	// 核心跳转管线 (Core Travel Pipeline)
	// ==============================================================================
public:

	// 极致瘦身：仅需传入目标地图名称，其余加载参数、UI全自动从大管家字典中获取
	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void ExecuteMapTravel(FName TargetLevelName);

	// 跨地图与同地图漫游的终极收尾解穴函数：恢复玩家输入与物理碰撞
	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void RestorePlayerInput();

	// 【新增】：剥离出来的独立接口，专职负责在绝对黑幕下执行物理空间折叠！
	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void SnapPlayerToDestination();


	// ==============================================================================
	// 动态滑动窗口与流送管线 (Dynamic Sliding Window & Streaming Pipeline)
	// ==============================================================================
public:

	// 注册关卡流送序列，建立远近维度的滑动窗口映射表
	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void RegisterZoneSequence(const TArray<FZoneDataLayerPair>& InSequence);

	// 根据当前触发的数据层，自动推演并刷新周围距离为 0、1、>=2 的数据层内存状态
	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void RefreshSlidingWindow(UDataLayerAsset* TriggeredLayer);

	// 将远景艺术层加载进内存但不激活其物理逻辑，充当无缝视野缓冲区
	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void PreheatZoneBackground(const UDataLayerAsset* ArtLayerAsset);

	// 彻底唤醒目标玩法层和艺术层，开始渲染并启用关卡内部的逻辑与碰撞
	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void ActivateZoneGameplay(const UDataLayerAsset* GameplayLayerAsset, const UDataLayerAsset* ArtLayerAsset);

	// 将指定的数据层物理级卸载出内存，强制释放占用
	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void EliminateZone(const UDataLayerAsset* LayerToUnload);

	// 在屏幕上打印当前序列中所有艺术层与玩法层的真实底层内存加载状态
	UFUNCTION(BlueprintCallable, Category = "MapTravel|Debug")
	void DebugPrintDataLayerStates();


	// ==============================================================================
	// 内部状态锁 (Internal State Locks)
	// ==============================================================================
private:

	// 转场状态互斥锁，防止玩家在转场期间重复触发导致时序错乱
	UPROPERTY()
	bool bIsTraveling = false;

	// 记录上一次激活的数据层，防抖拦截重复踩踏触发器
	UPROPERTY()
	TObjectPtr<UDataLayerAsset> LastActiveZone = nullptr;

	// 缓存 World Partition 数据层管理器，避免运行时高频查询
	TWeakObjectPtr<UDataLayerManager> CachedDataLayerManager;

	// 【新增】：缓存同地图传送的目标数据层，等待黑幕时机进行斩杀替换
	UPROPERTY()
	TObjectPtr<UDataLayerAsset> PendingSameMapDataLayer = nullptr;

	// 滑动窗口流送序列缓存，由关卡蓝图在初始化时推入
	UPROPERTY()
	TArray<FZoneDataLayerPair> ZoneSequence;

	// 高速本地路由字典：以路由资产为 Key，缓存当前大世界内存中所有已就绪的接机点注册信息
	UPROPERTY(Transient)
	TMap<UTeleportRoute*, FDestinationRegistrationInfo> SameMapDestinationRegistry;
};
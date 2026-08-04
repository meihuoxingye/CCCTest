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
 * 专职统筹物理坐标转移、黑屏遮罩与输入控制权交接
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
	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void RegisterSameMapDestination(UTeleportRoute* Route, AActor* DestinationActor, const FTransform& TargetTransform, UDataLayerAsset* BoundDataLayer = nullptr);

	// 安全注销接口：目标点被物理销毁时调用，从内存字典中清洗自身，防野指针崩溃
	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void UnregisterSameMapDestination(UTeleportRoute* Route);

	// 大一统传送门户：由传送门调用，系统自动判别走同地图极速瞬移还是跨地图无缝流送
	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void ExecuteUniversalTravel(AActor* TeleportingActor, UTeleportRoute* TargetRoute);

	// 专门供 GameInstance 瞬移落地的同一物理帧调用的斩杀接口
	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void CommitSameMapDataLayer();

	// 同地图专属执行实体：接管同图时空流速，执行绝对物理点穴，随后将坐标抛给大管家折叠
	void ExecuteSameMapTravel(AActor* TeleportingActor, UTeleportRoute* TargetRoute);

	// ==============================================================================
	// 核心跳转管线 (Core Travel Pipeline)
	// ==============================================================================
public:
	// 【核心功能】：大一统漫游管线的终极总闸，全自动统筹物理点穴、黑幕遮罩、时序挂起及最终的跨界跳跃传送。
	// 极致瘦身：仅需传入目标地图名称，其余加载参数、UI全自动从大管家字典中获取。
	// 【架构进化】：彻底废弃基于字符串盲猜的“智能分流”，改为由调用方明确下达跳转指令，消除底层隐患。
	// 本函数专职负责【默认普通单机跨图】，强制走无缝漫游 (Seamless Travel，bAbsolute = false)。
	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void ExecuteMapTravel(FName TargetLevelName);

	// 【联机专线】：房主专属建房跳转（带队发车）
	// 强制剥夺旧世界的无缝漫游面具，使用绝对跳转 (Absolute Travel) 从零创建网络驱动，并自动附加 ?listen 监听参数。
	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void ExecuteHostTravel(FName TargetLevelName);

	// 【联机专线】：客户端专属飞线加入
	// 绕过所有地图查表机制，拿着纯净的 P2P 隧道地址 (如 "[EOS:0002d6...]")，强制使用 ClientTravel 飞线接入房主主机。
	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void ExecuteClientJoin(const FString& ConnectString);

	// 跨地图与同地图漫游的终极收尾解穴函数：恢复玩家输入与物理碰撞
	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void RestorePlayerInput();

	// 剥离出来的独立接口，专职负责在绝对黑幕下执行物理空间折叠！
	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void SnapPlayerToDestination();

private:
	// 内部大一统收束管线：抹平单机、房主、客机在点穴、剥夺输入和黑幕掩护上的底层差异。
	// 统一执行极其严格的物理级时序锁定，最后再根据指令精准呼叫虚幻的底层 ServerTravel 或 ClientTravel API。
	void InternalExecuteTravel(FName TargetLevelName, const FString& TravelURL, bool bIsAbsolute, bool bIsClientJoin);

	// ==============================================================================
	// 内部状态锁 (Internal State Locks)
	// ==============================================================================
private:
	// 转场状态互斥锁，防止玩家在转场期间重复触发导致时序错乱
	UPROPERTY()
	bool bIsTraveling = false;

	// 缓存同地图传送的目标数据层，等待黑幕时机跨系统交给流送子系统进行斩杀替换
	UPROPERTY()
	TObjectPtr<UDataLayerAsset> PendingSameMapDataLayer = nullptr;

	// 高速本地路由字典：以路由资产为 Key，缓存当前大世界内存中所有已就绪的接机点注册信息
	UPROPERTY(Transient)
	TMap<UTeleportRoute*, FDestinationRegistrationInfo> SameMapDestinationRegistry;
};
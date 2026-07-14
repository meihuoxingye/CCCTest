// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MyMapTravelSubsystem.generated.h"

class UDataLayerAsset;
class UDataLayerManager;
class UMyBiomeConfig;
class ADirectionalLight;
class AExponentialHeightFog;


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
	// 核心跳转管线 (Core Travel Pipeline)
	// ==============================================================================
public:

	// 极致瘦身：仅需传入目标地图名称，其余加载参数、UI全自动从大管家字典中获取
	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void ExecuteMapTravel(FName TargetLevelName);

	// 跨地图与同地图漫游的终极收尾解穴函数：恢复玩家输入与物理碰撞
	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void RestorePlayerInput();


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

	// 平滑切断旧环境音效与雾气，异步过渡到目标生态的环境渲染参数
	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void UpdateEnvironment(UMyBiomeConfig* NewBiome, ADirectionalLight* MainLight, AExponentialHeightFog* MainFog);

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
	UDataLayerAsset* LastActiveZone = nullptr;

	// 缓存 World Partition 数据层管理器，避免运行时高频查询
	TWeakObjectPtr<UDataLayerManager> CachedDataLayerManager;

	// 滑动窗口流送序列缓存，由关卡蓝图在初始化时推入
	UPROPERTY()
	TArray<FZoneDataLayerPair> ZoneSequence;

	// 当前正在过渡的生态环境目标配置
	UPROPERTY()
	UMyBiomeConfig* CurrentBiomeTarget = nullptr;

	// 缓存的主光源弱指针，防范目标光源被外力物理销毁导致野指针崩溃
	TWeakObjectPtr<ADirectionalLight> CachedSunLight;

	// 缓存的大气雾弱指针
	TWeakObjectPtr<AExponentialHeightFog> CachedAtmosphereFog;

	// 驱动生态环境平滑渐变的后台异步定时器句柄
	FTimerHandle BiomeLerpTimer;

	// 生态环境渐变的当前进度 (0.0 到 1.0)
	float LerpAlpha = 0.0f;

	// 根据配置计算出的每次 Tick 的 Alpha 增量步长
	float LerpStep = 0.02f;

	// 记录生态渐变起点的太阳光旋转角度
	FRotator StartSunRotation;

	// 记录生态渐变起点的太阳光颜色
	FLinearColor StartSunColor;

	// 生态渐变定时器的 Tick 回调函数
	void ProcessBiomeLerpTick();


	// ==============================================================================
	// 同地图硬切换管线 (Intra-Map Hard Travel)
	// ==============================================================================
public:

	// 带有黑幕 UI 与强制闭眼等待的同地图瞬移切换管线（用于同地图内进 Boss 房等）
	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void ExecuteSameMapTravel(AActor* TeleportingActor, const FTransform& TargetTransform);

private:

	// 缓存同地图漫游期间创建的加载遮罩 UI
	UPROPERTY()
	class UUserWidget* ZoneLoadingWidget = nullptr;

	// 驱动同地图瞬移闭眼、等待及后续管线的定时器句柄
	FTimerHandle ZoneTravelTimerHandle;

	// 同地图漫游第二阶段回调：通知大管家撤下 UI 并触发恢复输入
	UFUNCTION()
	void FinishSameMapTravel();
};
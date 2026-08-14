// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MyDataLayerStreamingSubsystem.generated.h"

class UDataLayerAsset;
class UDataLayerManager;

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
class CCC_API UMyDataLayerStreamingSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

	// ==============================================================================
	// 核心生命周期与初始化 (Lifecycle & Initialization)
	// ==============================================================================
public:
	// 覆写子系统原生初始化函数，在世界创建且子系统被拉起时执行
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// 覆写子系统原生反初始化函数，在世界销毁或子系统卸载时清理内存
	virtual void Deinitialize() override;

	// ==============================================================================
	// 动态滑动窗口与流送管线 (Dynamic Sliding Window & Streaming Pipeline)
	// ==============================================================================
public:
	// 【蓝图调用规范】：必须在每个具体关卡蓝图 (Level Blueprint) 的 BeginPlay 中首发调用此节点！
	// 架构意义：C++ 仅提供底层的数学推演与内存流送引擎，关卡蓝图则作为“数据配置文件”。
	// 必须由关卡设计师将当前地图专属的区域序列（ZoneSequence）喂给本系统，实现纯数据驱动 (Data-Driven)。
	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void RegisterZoneSequence(const TArray<FZoneDataLayerPair>& InSequence);

	// 根据当前触发的数据层，自动推演并刷新周围距离为 0、1、>=2 的数据层内存状态。
	// 【蓝图调用规范】：在关卡蓝图 BeginPlay 调用完 RegisterZoneSequence 后，必须紧接着调用一次此节点，
	// 将玩家出生点所在的数据层传入，完成第一波初始地块的物理加载（防止开局掉入虚空）。
	// TriggeredLayer: 当前玩家踩中、或即将传送前往的核心数据层（靶心）。
	// bIsTeleporting: 是否处于黑幕传送掩护下。如果是，则强制刷新纹理 MIP；如果否（常规步行），则自然流送防卡顿。
	// - 不勾选 (False)：【常规跑图触发】或【关卡刚加载初始落地时】使用。绝对禁止调用纹理强刷指令，依靠引擎流送器自然演进，保障帧率绝对平滑，无卡顿感。
	// - 勾选 (True)：【仅限系统级传送】！只有在大管家执行同图/跨图的绝对黑屏掩护下（C++内部调用）才允许为 True。利用黑幕盲区强制压榨显卡刷新 MIP 层级，消除落地后远景模糊的穿帮问题。
	UFUNCTION(BlueprintCallable, Category = "MapTravel")
	void RefreshSlidingWindow(UDataLayerAsset* TriggeredLayer, bool bIsTeleporting = false);

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


	// 统筹判断并解析初始开荒数据层（决定是加载预设假人还是在底层彻底物理卸载）
	// 返回 true 代表是开荒，false 代表是回访
	bool ResolveStarterDataLayer(const UDataLayerAsset* StarterLayer);


	// ==============================================================================
	// 内部状态锁与缓存 (Internal State Locks & Cache)
	// ==============================================================================
private:
	// 记录上一次激活的数据层，防抖拦截重复踩踏触发器
	UPROPERTY()
	TObjectPtr<UDataLayerAsset> LastActiveZone = nullptr;

	// 滑动窗口流送序列缓存，由关卡蓝图在初始化时推入
	UPROPERTY()
	TArray<FZoneDataLayerPair> ZoneSequence;


};
// Fill out your copyright notice in the Description page of Project Settings.

#include "Streaming/MyDataLayerStreamingSubsystem.h"
#include "Engine/World.h"
#include "WorldPartition/DataLayer/DataLayerAsset.h"
#include "WorldPartition/DataLayer/DataLayerManager.h"
#include "WorldPartition/DataLayer/DataLayerInstance.h"
#include "Engine/Engine.h"
#include "TimerManager.h" // 【新增】：引入引擎定时器管理器

#include "Game/MyGameInstance.h"

// ==============================================================================
// 【新增】：引入大世界核心图鉴与包名提取工具
// ==============================================================================
#include "World/MyMapAttributeDataAsset.h" 
#include "Misc/PackageName.h"
#include "Kismet/GameplayStatics.h"


// ==============================================================================
// 核心生命周期与初始化 (Lifecycle & Initialization)
// ==============================================================================
#pragma region

void UMyDataLayerStreamingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	// 调用父类初始化
	Super::Initialize(Collection);

	// 重置上一次激活的数据层为空
	LastActiveZone = nullptr;

	// 清空滑动窗口的数据层序列缓存
	ZoneSequence.Empty();
}

void UMyDataLayerStreamingSubsystem::Deinitialize()
{
	// 清空数据层序列，释放内存
	ZoneSequence.Empty();

	// 清除对旧世界数据层资产的引用
	LastActiveZone = nullptr;

	// 调用父类反初始化完成收尾
	Super::Deinitialize();
}

#pragma endregion

// ==============================================================================
// 动态滑动窗口与流送管线 (Dynamic Sliding Window & Streaming Pipeline)
// ==============================================================================
#pragma region

void UMyDataLayerStreamingSubsystem::RegisterZoneSequence(const TArray<FZoneDataLayerPair>& InSequence)
{
	// 缓存滑动窗口流送序列
	ZoneSequence = InSequence;
}

void UMyDataLayerStreamingSubsystem::RefreshSlidingWindow(UDataLayerAsset* TriggeredLayer, bool bIsTeleporting)
{
	// 防御性判定：未注册序列或者触发层为空，直接返回
	if (ZoneSequence.Num() == 0 || !TriggeredLayer) return;

	UWorld* World = GetWorld();
	if (!World) return;

	UDataLayerManager* DLManager = UDataLayerManager::GetDataLayerManager(World);

	// 【优化 B：DataLayerManager 的获取安全性（异步重试防线）】
	// 无缝漫游落地瞬间，管理器可能尚未完全绑定至 World，挂起 0.1s 后递归重试保证指令必达
	if (!DLManager)
	{
		FTimerHandle RetryTimer;
		TWeakObjectPtr<UMyDataLayerStreamingSubsystem> WeakThis(this);
		World->GetTimerManager().SetTimer(RetryTimer, [WeakThis, TriggeredLayer, bIsTeleporting]()
			{
				if (UMyDataLayerStreamingSubsystem* StrongThis = WeakThis.Get())
				{
					StrongThis->RefreshSlidingWindow(TriggeredLayer, bIsTeleporting);
				}
			}, 0.1f, false);
		return;
	}

	// 状态锁：如果踩到的是相同的层，防抖拦截，避免无意义的性能损耗
	if (LastActiveZone == TriggeredLayer) return;

	// 更新最后激活的数据层记录
	LastActiveZone = TriggeredLayer;

	// 查找当前触发层在序列中的索引
	int32 CurrentIdx = INDEX_NONE;
	for (int32 i = 0; i < ZoneSequence.Num(); ++i)
	{
		// 只要艺术层或玩法层其中之一匹配上，就认为找到了当前区域
		if (ZoneSequence[i].ArtLayer == TriggeredLayer || ZoneSequence[i].GameplayLayer == TriggeredLayer)
		{
			CurrentIdx = i;
			break;
		}
	}

	// 【优化 C：索引计算的安全性（越界兜底卸载）】
	// 如果玩家意外掉出地图或飞出了所有已注册的网格区域，彻底卸载所有数据层，消除内存溢出风险
	if (CurrentIdx == INDEX_NONE)
	{
		for (const FZoneDataLayerPair& Zone : ZoneSequence)
		{
			if (Zone.ArtLayer) DLManager->SetDataLayerRuntimeState(Zone.ArtLayer, EDataLayerRuntimeState::Unloaded);
			if (Zone.GameplayLayer) DLManager->SetDataLayerRuntimeState(Zone.GameplayLayer, EDataLayerRuntimeState::Unloaded);
		}
		return;
	}

	// 核心滑动窗口算法：遍历整个序列进行远近维度管理
	for (int32 i = 0; i < ZoneSequence.Num(); ++i)
	{
		// 提取当前遍历到的区域配置
		const FZoneDataLayerPair& Zone = ZoneSequence[i];

		// 计算它与玩家当前所在区域的绝对距离
		int32 Distance = FMath::Abs(i - CurrentIdx);

		if (Distance == 0)
		{
			// 距离为 0 (玩家脚下)：艺术层和玩法层必须全面激活
			if (Zone.ArtLayer) DLManager->SetDataLayerRuntimeState(Zone.ArtLayer, EDataLayerRuntimeState::Activated);
			if (Zone.GameplayLayer) DLManager->SetDataLayerRuntimeState(Zone.GameplayLayer, EDataLayerRuntimeState::Activated);
		}
		else if (Distance == 1)
		{
			// 距离为 1 (玩家隔壁)：艺术层加载进内存但不激活玩法，充当无缝视野和缓冲区
			if (Zone.ArtLayer) DLManager->SetDataLayerRuntimeState(Zone.ArtLayer, EDataLayerRuntimeState::Loaded);
			if (Zone.GameplayLayer) DLManager->SetDataLayerRuntimeState(Zone.GameplayLayer, EDataLayerRuntimeState::Unloaded);
		}
		else
		{
			// 距离 >= 2 (远方区域)：完全从内存中物理卸载，极限节省内存
			if (Zone.ArtLayer) DLManager->SetDataLayerRuntimeState(Zone.ArtLayer, EDataLayerRuntimeState::Unloaded);
			if (Zone.GameplayLayer) DLManager->SetDataLayerRuntimeState(Zone.GameplayLayer, EDataLayerRuntimeState::Unloaded);
		}
	}

	// 【优化 A：纹理强制更新的开销控制】
	// 仅在黑幕传送掩护下强制刷新显存；正常步行探索时消除此指令，依靠引擎流送器自然更新以防止渲染卡顿 (Stall)
	if (bIsTeleporting && GEngine)
	{
		GEngine->Exec(World, TEXT("r.TextureStreaming.ForceUpdate"));
	}
}

void UMyDataLayerStreamingSubsystem::PreheatZoneBackground(const UDataLayerAsset* ArtLayerAsset)
{
	// 安全校验通过后，将对应的艺术层仅载入内存进行预热，不激活其内部的逻辑物理
	if (ArtLayerAsset)
	{
		if (UDataLayerManager* DLManager = UDataLayerManager::GetDataLayerManager(GetWorld()))
		{
			DLManager->SetDataLayerRuntimeState(ArtLayerAsset, EDataLayerRuntimeState::Loaded);
		}
	}
}

void UMyDataLayerStreamingSubsystem::ActivateZoneGameplay(const UDataLayerAsset* GameplayLayerAsset, const UDataLayerAsset* ArtLayerAsset)
{
	if (UDataLayerManager* DLManager = UDataLayerManager::GetDataLayerManager(GetWorld()))
	{
		// 彻底唤醒目标艺术层，开始渲染并启用碰撞
		if (ArtLayerAsset)
		{
			DLManager->SetDataLayerRuntimeState(ArtLayerAsset, EDataLayerRuntimeState::Activated);
		}

		// 彻底唤醒目标玩法层，敌人生成器和触发器等开始工作
		if (GameplayLayerAsset)
		{
			DLManager->SetDataLayerRuntimeState(GameplayLayerAsset, EDataLayerRuntimeState::Activated);
		}
	}
}

void UMyDataLayerStreamingSubsystem::EliminateZone(const UDataLayerAsset* LayerToUnload)
{
	// 将指定的数据层物理级卸载出内存，强制释放占用
	if (LayerToUnload)
	{
		if (UDataLayerManager* DLManager = UDataLayerManager::GetDataLayerManager(GetWorld()))
		{
			DLManager->SetDataLayerRuntimeState(LayerToUnload, EDataLayerRuntimeState::Unloaded);
		}
	}
}

void UMyDataLayerStreamingSubsystem::DebugPrintDataLayerStates()
{
	// 获取世界数据层管理器
	UDataLayerManager* DLManager = UDataLayerManager::GetDataLayerManager(GetWorld());
	if (!DLManager) return;

	// 打印雷达表头
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Yellow, TEXT("========== 数据层真实内存状态诊断 =========="));
	}

	// 倒序遍历序列，保证在屏幕上的输出顺序符合玩家直觉（从上到下即从近到远）
	for (int32 i = ZoneSequence.Num() - 1; i >= 0; --i)
	{
		const FZoneDataLayerPair& Zone = ZoneSequence[i];

		if (Zone.GameplayLayer)
		{
			// 获取底层真实的内存加载状态
			EDataLayerRuntimeState GPState = EDataLayerRuntimeState::Unloaded;
			if (const UDataLayerInstance* GPInstance = DLManager->GetDataLayerInstance(Zone.GameplayLayer))
			{
				GPState = GPInstance->GetRuntimeState();
			}

			// 状态文本转换
			FString StateStr = (GPState == EDataLayerRuntimeState::Activated) ? TEXT("已激活 (Activated)") :
				(GPState == EDataLayerRuntimeState::Loaded) ? TEXT("仅加载 (Loaded)") : TEXT("已卸载 (Unloaded)");

			// 警告色标红处理
			FColor MsgColor = (GPState == EDataLayerRuntimeState::Activated) ? FColor::Red : FColor::White;
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.f, MsgColor, FString::Printf(TEXT("Zone %d [玩法 Gameplay]: %s"), i, *StateStr));
		}

		if (Zone.ArtLayer)
		{
			// 获取底层真实的内存加载状态
			EDataLayerRuntimeState ArtState = EDataLayerRuntimeState::Unloaded;
			if (const UDataLayerInstance* ArtInstance = DLManager->GetDataLayerInstance(Zone.ArtLayer))
			{
				ArtState = ArtInstance->GetRuntimeState();
			}

			// 状态文本转换
			FString StateStr = (ArtState == EDataLayerRuntimeState::Activated) ? TEXT("已激活 (Activated)") :
				(ArtState == EDataLayerRuntimeState::Loaded) ? TEXT("仅加载 (Loaded)") : TEXT("已卸载 (Unloaded)");

			// 安全色标绿处理
			FColor MsgColor = (ArtState == EDataLayerRuntimeState::Activated) ? FColor::Green : FColor::White;
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.f, MsgColor, FString::Printf(TEXT("Zone %d [美术 Art]:      %s"), i, *StateStr));
		}
	}
}

bool UMyDataLayerStreamingSubsystem::ResolveStarterDataLayer(const UDataLayerAsset* StarterLayer)
{
	// 安全拦截：空资产直接驳回
	if (!StarterLayer) return false;

	UWorld* World = GetWorld();
	if (!World) return false;

	UMyGameInstance* GI = World->GetGameInstance<UMyGameInstance>();
	// 【架构统一】：严格使用 UE5.8 标准的 DataLayerManager
	UDataLayerManager* DLManager = UDataLayerManager::GetDataLayerManager(World);

	if (GI && DLManager)
	{
		// ==============================================================================
		// 💥 【核心修复】：强行剥离 PIE 前缀！
		// 使用 GetCurrentLevelName 并传入 true，彻底剥离 UEDPIE_0_ 前缀，还原纯净短名！
		// ==============================================================================
		FString CurrentMapName = World->GetMapName();
		FString CleanMapName = UGameplayStatics::GetCurrentLevelName(World, true);

		// 直接向大管家索要地图相性！大管家会自动遍历其资产大名单进行查表。
		EMapPhaseType MapType = GI->GetMapPhase(CleanMapName);

		// 终极仲裁：如果这张图根本没有开荒期（纯动态），或者开荒期已过（回访）！
		if (MapType == EMapPhaseType::AlwaysDynamic || GI->VisitedMaps.Contains(CurrentMapName))
		{
			// 回访地图/动态图：直接从底层将状态锁死为 Unloaded，彻底阻断初始角色（假人）的硬盘流送与实例化
			DLManager->SetDataLayerRuntimeState(StarterLayer, EDataLayerRuntimeState::Unloaded);
			UE_LOG(LogTemp, Warning, TEXT("🚫 [DataLayer管线] 地图 %s (动态/回访)，已物理阻断开荒预设层！"), *CurrentMapName);
			return false; // 明确汇报：这不是开荒，是回访/动态捏人
		}
		else
		{
			// 首次开荒：激活数据层，让关卡中手动摆放的角色正常加载，供玩家夺舍
			DLManager->SetDataLayerRuntimeState(StarterLayer, EDataLayerRuntimeState::Activated);
			// 将当前地图名称正式登记录入大管家的历史访问名单，打上“已开荒”烙印，防止下次进入产生双重肉体
			GI->VisitedMaps.Add(CurrentMapName);
			UE_LOG(LogTemp, Warning, TEXT("✅ [DataLayer管线] 首次进入 %s，初始预设角色已激活！"), *CurrentMapName);
			return true; // 明确汇报：这是纯正开荒
		}
	}
	return false;
}

#pragma endregion
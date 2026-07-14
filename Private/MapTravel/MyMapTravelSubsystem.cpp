// Fill out your copyright notice in the Description page of Project Settings.

#include "MapTravel/MyMapTravelSubsystem.h"
#include "Engine/World.h"
#include "WorldPartition/DataLayer/DataLayerAsset.h"
#include "WorldPartition/DataLayer/DataLayerManager.h"
#include "MapTravel/DataAsset/MyBiomeConfig.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/LightComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundMix.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"

#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "Framework/Application/SlateApplication.h" 
#include "Engine/GameViewportClient.h"

#include "Game/MyGameModeBase.h"
#include "Character/TopCharacter.h"
#include "EnhancedInputComponent.h"

#include "GameFramework/PlayerState.h"
#include "Blueprint/UserWidget.h"

#include "Game/MyGameInstance.h"
#include "Misc/PackageName.h"

#include "RenderingThread.h"
#include "HAL/IConsoleManager.h"

#include "Component/TimeDilationHubComponent.h"

#include "MapTravel/DataAsset/TeleportRoute.h"


// 引入基类类型以供强制传参
#include "UI/Transition/MyTransitionWidgetBase.h"

// ==============================================================================
// 内部安全获取真实玩家控制器的工具函数 (防无缝传送假身)
// ==============================================================================
namespace
{
	APlayerController* GetRealPlayerController(UWorld* World)
	{
		// 防御性编程：世界指针为空直接返回
		if (!World) return nullptr;

		// 遍历当前世界中所有的玩家控制器
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			// 获取当前的 PlayerController 指针
			APlayerController* PC = It->Get();

			// 1. PC 必须有效
			// 2. 必须是 LocalController (防止联机模式下拿到远程客户端的代理控制器)
			// 3. 必须拥有 LocalPlayer (防止无缝漫游中，拿到还没被销毁的残留假身或幽灵)
			if (PC && PC->IsLocalController() && PC->GetLocalPlayer())
			{
				// 找到了唯一合法的真实本地控制器，返回
				return PC;
			}
		}

		// 没找到则返回空
		return nullptr;
	}
}

// ==============================================================================
// 生命周期与初始化 (Lifecycle & Initialization)
// ==============================================================================
#pragma region

void UMyMapTravelSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	// 调用父类初始化
	Super::Initialize(Collection);

	// 安全获取当前世界上下文
	if (UWorld* World = GetWorld())
	{
		// 缓存 DataLayerManager 指针，避免运行时高频调用造成的开销
		CachedDataLayerManager = UDataLayerManager::GetDataLayerManager(World);
	}

	// 物理重置所有内部状态机参数与转场锁
	bIsTraveling = false;

	// 重置上一次激活的数据层为空
	LastActiveZone = nullptr;

	// 重置生态环境渐变的 Alpha 值为 0
	LerpAlpha = 0.0f;

	// 初始化默认的渐变步长
	LerpStep = 0.02f;

	// 清空滑动窗口的数据层序列缓存
	ZoneSequence.Empty();
}

void UMyMapTravelSubsystem::Deinitialize()
{
	// 释放弱指针缓存，防止旧世界的管理器变成野指针
	CachedDataLayerManager.Reset();

	// 清空数据层序列，释放内存
	ZoneSequence.Empty();

	// 清除对旧世界数据层资产的引用
	LastActiveZone = nullptr;

	// 安全获取当前世界
	if (UWorld* World = GetWorld())
	{
		// 防内存泄漏：清除本子系统挂在 TimerManager 上的所有定时器
		World->GetTimerManager().ClearAllTimersForObject(this);
	}

	// 调用父类反初始化完成收尾
	Super::Deinitialize();
}

void UMyMapTravelSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	// 调用父类逻辑
	Super::OnWorldBeginPlay(InWorld);

	// 获取当前刚落地的世界地图的原始长名称
	FString CurrentMapName = InWorld.GetMapName();

	// 获取大管家 GameInstance
	if (UMyGameInstance* GI = InWorld.GetGameInstance<UMyGameInstance>())
	{
		// 提取纯净的地图短名用于对比
		FString CleanTargetMap = FPackageName::GetShortName(GI->PendingTargetMapName.ToString());
		FString CleanCurrentMap = FPackageName::GetShortName(CurrentMapName);

		// 过滤过渡地图：如果没有挂起的目标，或者当前落地地图不是期望地图
		if (GI->PendingTargetMapName.IsNone() || !CleanCurrentMap.Contains(CleanTargetMap))
		{
			// 直接返回，不执行后续新世界的任何初始化逻辑
			return;
		}
	}
}

#pragma endregion


// ==============================================================================
// 核心跳转管线 (Core Travel Pipeline)
// ==============================================================================
#pragma region

void UMyMapTravelSubsystem::ExecuteMapTravel(FName TargetLevelName)
{
	if (bIsTraveling) return;
	bIsTraveling = true;

	UWorld* World = GetWorld();
	if (!IsValid(World) || TargetLevelName.IsNone())
	{
		bIsTraveling = false;
		return;
	}

	// 全局物理破片盾牌：第一时间恢复法则，防止转场 Timer 因全局变慢而被无限拉长
	UGameplayStatics::SetGlobalTimeDilation(World, 1.0f);

	if (GEngine && GEngine->GameViewport)
	{
		// 瞬间剥夺物理输入控制权
		GEngine->GameViewport->SetIgnoreInput(true);

		// 清空键盘焦点，阻断残余UI按键
		FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::Cleared);

		// 释放所有指针捕获，防止鼠标锁定导致UI失效
		FSlateApplication::Get().ReleaseAllPointerCapture();
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			// 时间领主防线：只对灵魂进行操作，安全关闭时间组件内部状态机
			if (UTimeDilationHubComponent* TimeComp = PC->GetComponentByClass<UTimeDilationHubComponent>())
			{
				TimeComp->ForceResetTime();
			}

			// 消除灵魂个体的流速残留，防止影响新世界
			PC->CustomTimeDilation = 1.0f;

			// 清理该控制器上绑定的全部定时器，切断过场期间的异步干扰
			World->GetTimerManager().ClearAllTimersForObject(PC);

			TArray<UActorComponent*> UIComponents;
			PC->GetComponents(UIComponents);
			for (UActorComponent* Comp : UIComponents)
			{
				// 清理控制器下属组件的定时器，防止组件在后台作妖
				World->GetTimerManager().ClearAllTimersForObject(Comp);
			}

			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
			{
				// 清空增强输入系统的映射上下文，防止黑屏期间发生轴输入叠加
				Subsystem->ClearAllMappings();
			}

			// 物理层清除粘连按键状态
			PC->FlushPressedKeys();

			// 逻辑层点穴：禁止控制器接受任何输入，绝不调用 UnPossess 以维持附身关系
			PC->DisableInput(PC);
			PC->SetIgnoreMoveInput(true);
			PC->SetIgnoreLookInput(true);

			if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PC->InputComponent))
			{
				// 清空控制器输入组件的一切动作绑定
				EIC->ClearActionBindings();
			}

			if (APawn* PlayerPawn = PC->GetPawn())
			{
				// 肉体只配被重置个体时间，绝不去肉体蓝图里找全局时间组件
				PlayerPawn->CustomTimeDilation = 1.0f;

				// 物理层点穴：关闭碰撞防止穿模或掉落，摄像机完美维持原机位
				PlayerPawn->SetActorEnableCollision(false);

				if (UEnhancedInputComponent* PawnEIC = Cast<UEnhancedInputComponent>(PlayerPawn->InputComponent))
				{
					// 清空肉体输入组件的一切动作绑定
					PawnEIC->ClearActionBindings();
				}
			}

			if (PC->GetLevel() != World->PersistentLevel)
			{
				// 重迎免死金牌：强行将控制器挂载到持久关卡，防 World Partition 导致 0x30 闪退
				PC->Rename(nullptr, World->PersistentLevel);
			}

			if (PC->PlayerState && PC->PlayerState->GetLevel() != World->PersistentLevel)
			{
				// 重迎免死金牌：强行将玩家状态挂载到持久关卡，防无缝漫游期间数据链断裂
				PC->PlayerState->Rename(nullptr, World->PersistentLevel);
			}
		}
	}

	UMyGameInstance* GI = World->GetGameInstance<UMyGameInstance>();
	if (!GI)
	{
		bIsTraveling = false;
		return;
	}

	FName CurrentMapName = FName(*UGameplayStatics::GetCurrentLevelName(World, true));
	FMapTransitionConfig OutConfig = GI->GetMapTransitionConfig(CurrentMapName);

	// 核心数据驱动：直接记下目标地图名，落地后大管家自己会根据名字去查字典
	GI->PendingTargetMapName = TargetLevelName;

	// 通过大管家配置拉起熄屏 UI
	GI->PlayScreenOffUI(OutConfig.ScreenOffUIClass, OutConfig.ScreenOffDuration);

	float SafeTravelDelay = FMath::Max(GI->SystemSafeDelay, OutConfig.ScreenOffDuration);

	FTimerHandle TravelTimerHandle;

	// 使用 Lambda 挂起定时器，等待熄屏动画播完后精确执行 ServerTravel 降维打击
	World->GetTimerManager().SetTimer(TravelTimerHandle, [World, TargetLevelName]()
		{
			if (IsValid(World))
			{
				// 真正起航：命令引擎底层开启无缝旅行管线
				World->ServerTravel(TargetLevelName.ToString());
			}
		}, SafeTravelDelay, false);
}

void UMyMapTravelSubsystem::RestorePlayerInput()
{
	// 安全获取世界上下文
	UWorld* World = GetWorld();
	if (!World) return;

	// 彻底解除视口级别的输入忽略屏蔽
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->SetIgnoreInput(false);
	}

	// 核心解穴：安全获取本地真实的玩家控制器
	if (APlayerController* PC = GetRealPlayerController(World))
	{
		// 恢复控制器本身的输入响应
		PC->EnableInput(PC);

		// 清理黑幕期间玩家盲按留下的残留物理按键状态
		PC->FlushPressedKeys();

		// 恢复为正常的游戏与UI混合输入模式
		FInputModeGameAndUI InputMode;

		// 解除鼠标对视口的强制锁定
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);

		// 确保捕获期间不隐藏鼠标光标
		InputMode.SetHideCursorDuringCapture(false);

		// 应用输入模式
		PC->SetInputMode(InputMode);

		// 恢复肉体的响应
		if (APawn* Pawn = PC->GetPawn())
		{
			// 恢复肉体的按键接收
			Pawn->EnableInput(PC);

			// 恢复肉体的物理碰撞
			Pawn->SetActorEnableCollision(true);
		}

		// 强制将底层的用户焦点重新对齐到游戏视口
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
	}

	// ================= 追加以下代码 =================
	// 彻底完成管线，销毁跨界车票防幽灵复活
	if (UMyGameInstance* GI = World->GetGameInstance<UMyGameInstance>())
	{
		GI->PendingTravelRoute = nullptr;
	}

	// 彻底完成管线，解除转场锁
	bIsTraveling = false;
}

#pragma endregion


// ==============================================================================
// 大一统传送路由中心 (Universal Routing Hub)
// ==============================================================================
#pragma region

void UMyMapTravelSubsystem::RegisterSameMapDestination(UTeleportRoute* Route, AActor* DestinationActor, const FTransform& TargetTransform)
{
	if (!Route || !DestinationActor) return;

	// 架构级防呆：捕获并镇压由于关卡策划配置失误导致的多对一冲突
	if (SameMapDestinationRegistry.Contains(Route))
	{
		FDestinationRegistrationInfo ConflictedInfo = SameMapDestinationRegistry[Route];
		FString OldActorName = ConflictedInfo.RegistrySource.IsValid() ? ConflictedInfo.RegistrySource->GetName() : TEXT("已失效实体");
		FString NewActorName = DestinationActor->GetName();

		// 向开发者输出极度醒目的红色警告阵列，强制暴露脏数据
		UE_LOG(LogTemp, Error, TEXT("=========================================================================="));
		UE_LOG(LogTemp, Error, TEXT("[大一统传送系统] 致命冲突！路由资产 [%s] 被多个目标点同时监听！"), *Route->GetName());
		UE_LOG(LogTemp, Error, TEXT(" -> 已注册生效的守卫点: [%s]"), *OldActorName);
		UE_LOG(LogTemp, Error, TEXT(" -> 试图二次篡改的侵入点: [%s] (此入侵已被系统物理隔离并抛弃！)"), *NewActorName);
		UE_LOG(LogTemp, Error, TEXT("=========================================================================="));
		return;
	}

	// 组装合法数据并注入本地高速字典
	FDestinationRegistrationInfo NewInfo;
	NewInfo.RegistrySource = DestinationActor;
	NewInfo.TargetTransform = TargetTransform;

	SameMapDestinationRegistry.Add(Route, NewInfo);
}

void UMyMapTravelSubsystem::UnregisterSameMapDestination(UTeleportRoute* Route)
{
	if (Route)
	{
		SameMapDestinationRegistry.Remove(Route);
	}
}

void UMyMapTravelSubsystem::ExecuteUniversalTravel(AActor* TeleportingActor, UTeleportRoute* TargetRoute)
{
	if (!TeleportingActor || !TargetRoute) return;

	// 第一路由优先级：如果目标路由的接机点在当前内存字典中，直接劫持为同地图极速穿梭
	if (SameMapDestinationRegistry.Contains(TargetRoute))
	{
		ExecuteSameMapTravel(TeleportingActor, TargetRoute);
		return;
	}

	// 第二路由优先级：本地查无此人，断定为跨界航行，查阅资产内部的目标地图
	UWorld* World = GetWorld();
	if (!World) return;

	if (UMyGameInstance* GI = World->GetGameInstance<UMyGameInstance>())
	{
		if (!TargetRoute->TargetMap.IsNull())
		{
			// 铸造跨界车票并交由全局大管家保管
			GI->PendingTravelRoute = TargetRoute;

			// 从软引用萃取真实地图包名，移交跨地图无缝流送管线
			FString TargetMapName = TargetRoute->TargetMap.GetAssetName();
			ExecuteMapTravel(*TargetMapName);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[大一统传送系统] 寻路瘫痪！路由资产 [%s] 既不在本图监听字典中，也未配置目标跨界地图！"), *TargetRoute->GetName());
		}
	}
}

/*
AActor* UMyMapTravelSubsystem::GetDestinationActor(UTeleportRoute* Route)
{
	// 极速 O(1) 字典寻址
	if (Route && SameMapDestinationRegistry.Contains(Route))
	{
		return SameMapDestinationRegistry[Route].RegistrySource.Get();
	}
	return nullptr;
}
*/

void UMyMapTravelSubsystem::ExecuteSameMapTravel(AActor* TeleportingActor, UTeleportRoute* TargetRoute)
{
	if (bIsTraveling || !TeleportingActor || !TargetRoute) return;
	if (!SameMapDestinationRegistry.Contains(TargetRoute)) return;

	bIsTraveling = true;

	UWorld* World = GetWorld();
	if (!World)
	{
		bIsTraveling = false;
		return;
	}

	// 提取绝对物理坐标
	FTransform TargetTransform = SameMapDestinationRegistry[TargetRoute].TargetTransform;

	// 物理层静默：强制抹平时空流速，彻底剥夺玩家控制权
	UGameplayStatics::SetGlobalTimeDilation(World, 1.0f);

	if (APlayerController* PC = GetRealPlayerController(World))
	{
		PC->CustomTimeDilation = 1.0f;
		PC->DisableInput(PC);

		FInputModeUIOnly InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);

		if (APawn* Pawn = PC->GetPawn())
		{
			Pawn->CustomTimeDilation = 1.0f;
			Pawn->DisableInput(PC);
		}
	}

	UMyGameInstance* GI = World->GetGameInstance<UMyGameInstance>();
	if (!GI)
	{
		bIsTraveling = false;
		return;
	}

	// 【替换为这行：彻底剥离 PIE 前缀，让 UI 顺利加载，接管解穴流程！】
	FName CurrentMapName = FName(*UGameplayStatics::GetCurrentLevelName(World, true));
	FMapTransitionConfig Config = GI->GetMapTransitionConfig(CurrentMapName);
	GI->PendingTargetMapName = CurrentMapName;
	GI->PlayScreenOffUI(Config.ScreenOffUIClass, Config.ScreenOffDuration);

	// 核心修复 1：强制设定 0.01 秒的最小底线，坚决防止虚幻 SetTimer 遇到 0.0 瞬间删除定时器引发永久锁死！
	float SafeDelay = FMath::Max(0.01f, FMath::Max(GI->SystemSafeDelay, Config.ScreenOffDuration));
	float IntroDelay = FMath::Max(0.01f, Config.MinLoadingTime);

	FTimerDelegate TimerDel;
	TimerDel.BindLambda([this, TeleportingActor, TargetTransform, IntroDelay]()
		{
			// 核心修复 2：废弃 SetActorTransform，改用最高物理权限的 TeleportTo，绝对防止玩家跟地板穿模卡死
			if (IsValid(TeleportingActor))
			{
				TeleportingActor->TeleportTo(TargetTransform.GetLocation(), TargetTransform.GetRotation().Rotator(), false, true);
			}

			if (UWorld* InnerWorld = GetWorld())
			{
				if (UMyGameInstance* InnerGI = InnerWorld->GetGameInstance<UMyGameInstance>())
				{
					InnerGI->ShowFakeLoadingScreen(nullptr);
				}

				InnerWorld->GetTimerManager().SetTimer(SameMapTravelTimerHandle, this, &UMyMapTravelSubsystem::FinishSameMapTravel, IntroDelay, false);
			}
		});

	World->GetTimerManager().SetTimer(SameMapTravelTimerHandle, TimerDel, SafeDelay, false);
}

void UMyMapTravelSubsystem::FinishSameMapTravel()
{
	if (UWorld* World = GetWorld())
	{
		if (UMyGameInstance* GI = World->GetGameInstance<UMyGameInstance>())
		{
			GI->HideFakeLoadingScreen();
		}
	}

	// 【必须补上这句】：同地图黑幕结束后，彻底恢复玩家的输入控制权！
	RestorePlayerInput();
}

#pragma endregion


// ==============================================================================
// 动态滑动窗口与流送管线 (Dynamic Sliding Window & Streaming Pipeline)
// ==============================================================================
#pragma region

void UMyMapTravelSubsystem::RegisterZoneSequence(const TArray<FZoneDataLayerPair>& InSequence)
{
	// 缓存滑动窗口流送序列
	ZoneSequence = InSequence;
}

void UMyMapTravelSubsystem::RefreshSlidingWindow(UDataLayerAsset* TriggeredLayer)
{
	// 获取数据层管理器
	UDataLayerManager* DLManager = UDataLayerManager::GetDataLayerManager(GetWorld());

	// 防御性判定：如果管理器无效，或者未注册序列，或者触发层为空，直接返回
	if (!DLManager || ZoneSequence.Num() == 0 || !TriggeredLayer) return;

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

	// 如果不在注册的序列中，直接返回
	if (CurrentIdx == INDEX_NONE) return;

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

	// 在主线程发出指令，强制纹理流送器立刻更新，防范远处模型在激活时出现模糊糊的低级 MIP
	if (GEngine && GetWorld())
	{
		GEngine->Exec(GetWorld(), TEXT("r.TextureStreaming.ForceUpdate"));
	}
}

void UMyMapTravelSubsystem::PreheatZoneBackground(const UDataLayerAsset* ArtLayerAsset)
{
	// 安全校验通过后，将对应的艺术层仅载入内存进行预热，不激活其内部的逻辑物理
	if (CachedDataLayerManager.IsValid() && ArtLayerAsset)
	{
		CachedDataLayerManager->SetDataLayerRuntimeState(ArtLayerAsset, EDataLayerRuntimeState::Loaded);
	}
}

void UMyMapTravelSubsystem::ActivateZoneGameplay(const UDataLayerAsset* GameplayLayerAsset, const UDataLayerAsset* ArtLayerAsset)
{
	if (CachedDataLayerManager.IsValid())
	{
		// 彻底唤醒目标艺术层，开始渲染并启用碰撞
		if (ArtLayerAsset)
		{
			CachedDataLayerManager->SetDataLayerRuntimeState(ArtLayerAsset, EDataLayerRuntimeState::Activated);
		}

		// 彻底唤醒目标玩法层，敌人生成器和触发器等开始工作
		if (GameplayLayerAsset)
		{
			CachedDataLayerManager->SetDataLayerRuntimeState(GameplayLayerAsset, EDataLayerRuntimeState::Activated);
		}
	}
}

void UMyMapTravelSubsystem::EliminateZone(const UDataLayerAsset* LayerToUnload)
{
	// 将指定的数据层物理级卸载出内存，强制释放占用
	if (CachedDataLayerManager.IsValid() && LayerToUnload)
	{
		CachedDataLayerManager->SetDataLayerRuntimeState(LayerToUnload, EDataLayerRuntimeState::Unloaded);
	}
}

void UMyMapTravelSubsystem::UpdateEnvironment(UMyBiomeConfig* NewBiome, ADirectionalLight* MainLight, AExponentialHeightFog* MainFog)
{
	// 防御性安全拦截
	if (!NewBiome || !MainLight || !MainLight->GetComponent()) return;
	UWorld* World = GetWorld();
	if (!World) return;

	// 平滑切断旧的环境音效与 BGM 修改器
	if (CurrentBiomeTarget && CurrentBiomeTarget->BiomeSoundMix)
	{
		UGameplayStatics::PopSoundMixModifier(World, CurrentBiomeTarget->BiomeSoundMix);
	}

	// 推送新的生态混音，引擎音频系统会自动处理淡入淡出
	if (NewBiome->BiomeSoundMix)
	{
		UGameplayStatics::PushSoundMixModifier(World, NewBiome->BiomeSoundMix);
	}

	// 锁定新的生态插值目标以及相关光源引用
	CurrentBiomeTarget = NewBiome;
	CachedSunLight = MainLight;
	CachedAtmosphereFog = MainFog;

	// 重置插值进度
	LerpAlpha = 0.0f;

	// 防止配置填错导致除零崩溃，最短过渡时间限制为 0.1 秒
	float Duration = FMath::Max(0.1f, NewBiome->TransitionDuration);

	// 根据定时器的固定触发频率 (0.05s) 计算每一步的 Alpha 增量
	LerpStep = 0.05f / Duration;

	// 记录平滑插值的物理起点
	StartSunRotation = MainLight->GetComponent()->GetComponentRotation();
	StartSunColor = MainLight->GetComponent()->LightColor;

	// 挂起后台高频定时器，开始以非阻塞方式异步过渡环境渲染
	World->GetTimerManager().SetTimer(BiomeLerpTimer, this, &UMyMapTravelSubsystem::ProcessBiomeLerpTick, 0.05f, true);
}

void UMyMapTravelSubsystem::DebugPrintDataLayerStates()
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

void UMyMapTravelSubsystem::ProcessBiomeLerpTick()
{
	// 防御链条：世界失效、目标配置丢失、或主光源被外力物理销毁时，立即自尽停止 Tick
	if (!IsValid(GetWorld()) || !CurrentBiomeTarget || !CachedSunLight.IsValid() || !CachedSunLight->GetComponent())
	{
		if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(BiomeLerpTimer);
		return;
	}

	// 累加计算插值进度
	LerpAlpha += LerpStep;

	// 钳制范围防溢出
	LerpAlpha = FMath::Clamp(LerpAlpha, 0.0f, 1.0f);

	UDirectionalLightComponent* LightComp = CachedSunLight->GetComponent();

	// 线性插值计算当前帧的物理状态
	FRotator NewRot = FMath::Lerp(StartSunRotation, CurrentBiomeTarget->TargetSunRotation, LerpAlpha);
	FLinearColor NewColor = FMath::Lerp(StartSunColor, CurrentBiomeTarget->SunLightColor, LerpAlpha);

	// 应用状态：驱动天光旋转（时间推移）与色彩渐变
	LightComp->SetWorldRotation(NewRot);
	LightComp->SetLightColor(NewColor);

	// 同步平滑处理大气雾的浓度
	if (CachedAtmosphereFog.IsValid() && CachedAtmosphereFog->GetComponent())
	{
		CachedAtmosphereFog->GetComponent()->SetFogDensity(
			FMath::Lerp(CachedAtmosphereFog->GetComponent()->FogDensity, CurrentBiomeTarget->FogDensity, LerpAlpha)
		);
	}

	// 检查是否到达终点
	if (LerpAlpha >= 1.0f)
	{
		// 功成身退，销毁后台定时器
		if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(BiomeLerpTimer);
	}
}

#pragma endregion
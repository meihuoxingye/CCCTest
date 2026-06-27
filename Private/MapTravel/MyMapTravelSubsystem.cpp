// Fill out your copyright notice in the Description page of Project Settings.

#include "MapTravel/MyMapTravelSubsystem.h"
#include "Engine/World.h"
#include "WorldPartition/DataLayer/DataLayerAsset.h"
#include "WorldPartition/DataLayer/DataLayerManager.h"
#include "MapTravel/MyBiomeConfig.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/LightComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "TimerManager.h"
#include "MoviePlayer.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundMix.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/Engine.h"

#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/GameViewportClient.h"

#include "Game/MyGameModeBase.h"
#include "Character/TopCharacter.h"
#include "EnhancedInputComponent.h"


// ==============================================================================
// 生命周期与初始化 (Lifecycle & Initialization)
// ==============================================================================
#pragma region

void UMyMapTravelSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UWorld* World = GetWorld())
	{
		CachedDataLayerManager = UDataLayerManager::GetDataLayerManager(World);
	}

	bIsTraveling = false;
	LastActiveZone = nullptr;
	LerpAlpha = 0.0f;
	LerpStep = 0.02f;
	ZoneSequence.Empty();
}

void UMyMapTravelSubsystem::Deinitialize()
{
	CachedDataLayerManager.Reset();
	ZoneSequence.Empty();
	LastActiveZone = nullptr;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BiomeLerpTimer);
	}

	Super::Deinitialize();
}

#pragma endregion

// ==============================================================================
// 核心跳转管线 (Core Travel Pipeline)
// ==============================================================================
#pragma region

void UMyMapTravelSubsystem::ExecuteMapTravel(FName TargetLevelName)
{
	if (bIsTraveling)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World) || TargetLevelName.IsNone())
	{
		return;
	}

	// ==============================================================================
	// 【终极病灶切除 1：焦土政策 (Scorched Earth Cleanup)】
	// ==============================================================================
	if (GEngine && GEngine->GameViewport)
	{
		// 1. 强行把引擎焦点砸回 3D 视口，无情剥夺所有旧 UI 的焦点权
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
		FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::Cleared);

		// 2. 拔掉当前屏幕上的所有 UI，使其随旧世界一起火化
		GEngine->GameViewport->RemoveAllViewportWidgets();
	}

	// ==============================================================================
	// 【终极病灶切除 2：异步任务大清洗 (Timer Purgatory)】
	// ==============================================================================
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			// 1. 清理 PC 本身所有的计时器
			World->GetTimerManager().ClearAllTimersForObject(PC);

			// 2. 遍历并清理该 PC 身上挂载的所有组件的计时器（彻底斩断 MyUIHandlerComponent 的 NextTick 诈尸）
			TArray<UActorComponent*> UIComponents;
			PC->GetComponents(UIComponents);
			for (UActorComponent* Comp : UIComponents)
			{
				World->GetTimerManager().ClearAllTimersForObject(Comp);
			}

			// 3. 常规输入清理
			PC->FlushPressedKeys();
			PC->DisableInput(PC);

			FInputModeGameOnly GameOnlyMode;
			PC->SetInputMode(GameOnlyMode);

			if (APawn* PlayerPawn = PC->GetPawn())
			{
				PlayerPawn->SetActorEnableCollision(false);
			}
		}
	}

	if (IsMoviePlayerEnabled())
	{
		FLoadingScreenAttributes LoadingScreen;
		LoadingScreen.bAutoCompleteWhenLoadingCompletes = true;
		LoadingScreen.bWaitForManualStop = false;
		LoadingScreen.bAllowEngineTick = false;
		LoadingScreen.WidgetLoadingScreen = FLoadingScreenAttributes::NewTestLoadingScreenWidget();
		GetMoviePlayer()->SetupLoadingScreen(LoadingScreen);
	}

	bIsTraveling = true;
	World->ServerTravel(TargetLevelName.ToString());
}

#pragma endregion


// ==============================================================================
// 动态滑动窗口与流送管线 (Dynamic Sliding Window & Streaming Pipeline)
// ==============================================================================
#pragma region

void UMyMapTravelSubsystem::RegisterZoneSequence(const TArray<FZoneDataLayerPair>& InSequence)
{
	ZoneSequence = InSequence;
}

void UMyMapTravelSubsystem::RefreshSlidingWindow(UDataLayerAsset* TriggeredLayer)
{
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, TEXT("[管家收到] 雷达信号已接入，开始流送前置检查..."));

	// 【破案防线 1】：动态获取 Manager，抛弃 Initialize 里的缓存，它经常失效！
	UDataLayerManager* DLManager = UDataLayerManager::GetDataLayerManager(GetWorld());
	if (!DLManager)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("[严重致命] 管家罢工：DataLayerManager 无效！关卡没开启世界分区？"));
		return;
	}

	// 【破案防线 2】：关卡蓝图有没有把数据送进来？
	if (ZoneSequence.Num() == 0)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Red, TEXT("[严重致命] 管家罢工：ZoneSequence 数组为空！你的关卡蓝图(Level Blueprint)连线断了，或者根本没执行 Register 节点！"));
		return;
	}

	if (!TriggeredLayer) return;

	// 极端回溯防抖
	if (LastActiveZone == TriggeredLayer) return;
	LastActiveZone = TriggeredLayer;

	// 定位当前所在关卡的索引
	int32 CurrentIdx = INDEX_NONE;
	for (int32 i = 0; i < ZoneSequence.Num(); ++i)
	{
		if (ZoneSequence[i].ArtLayer == TriggeredLayer || ZoneSequence[i].GameplayLayer == TriggeredLayer)
		{
			CurrentIdx = i;
			break;
		}
	}

	// 【破案防线 3】：填的资产和蓝图里的对不上号？
	if (CurrentIdx == INDEX_NONE)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, FString::Printf(TEXT("[严重致命] 雷达传来的资产 [%s] 根本不在关卡蓝图的数组里！两边填的不是同一个文件！"), *TriggeredLayer->GetName()));
		return;
	}

	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("[总管执行] 身份核对成功！锁定为第 %d 关，开始调度内存..."), CurrentIdx));

	// 双轨滑动窗口调度
	for (int32 i = 0; i < ZoneSequence.Num(); ++i)
	{
		const FZoneDataLayerPair& Zone = ZoneSequence[i];
		int32 Distance = FMath::Abs(i - CurrentIdx);

		if (Distance == 0)
		{
			if (Zone.ArtLayer) DLManager->SetDataLayerRuntimeState(Zone.ArtLayer, EDataLayerRuntimeState::Activated);
			if (Zone.GameplayLayer) DLManager->SetDataLayerRuntimeState(Zone.GameplayLayer, EDataLayerRuntimeState::Activated);
		}
		else if (Distance == 1)
		{
			if (Zone.ArtLayer) DLManager->SetDataLayerRuntimeState(Zone.ArtLayer, EDataLayerRuntimeState::Loaded);

			if (Zone.GameplayLayer)
			{
				SanitizeActorsForUnload(Zone.GameplayLayer);
				DLManager->SetDataLayerRuntimeState(Zone.GameplayLayer, EDataLayerRuntimeState::Unloaded);
			}
		}
		else
		{
			if (Zone.ArtLayer) DLManager->SetDataLayerRuntimeState(Zone.ArtLayer, EDataLayerRuntimeState::Unloaded);

			if (Zone.GameplayLayer)
			{
				SanitizeActorsForUnload(Zone.GameplayLayer);
				DLManager->SetDataLayerRuntimeState(Zone.GameplayLayer, EDataLayerRuntimeState::Unloaded);
			}
		}
	}

	if (GEngine && GetWorld())
	{
		GEngine->Exec(GetWorld(), TEXT("r.TextureStreaming.ForceUpdate"));
	}

	DebugPrintDataLayerStates();
}

void UMyMapTravelSubsystem::SanitizeActorsForUnload(const UDataLayerAsset* ZoneToUnload)
{
	UWorld* World = GetWorld();
	if (!World || !ZoneToUnload) return;

	if (AMyGameModeBase* GM = Cast<AMyGameModeBase>(World->GetAuthGameMode()))
	{
		for (int32 i = GM->FriendlyRoster.Num() - 1; i >= 0; --i)
		{
			ATopCharacter* Victim = Cast<ATopCharacter>(GM->FriendlyRoster[i]);

			if (Victim && Victim->GetDataLayerAssets().Contains(ZoneToUnload))
			{
				// -------------------------------------------------------------
				// 【硬核追踪日志 1：即将被献祭的角色状态】
				// -------------------------------------------------------------
				FString VictimName = Victim->GetName();
				UE_LOG(LogTemp, Warning, TEXT(">>> [卸载清扫] 目标角色: %s, 即将被 Zone 吞噬！"), *VictimName);
				if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Magenta, FString::Printf(TEXT(">>> [卸载清扫] 正在处理被吞噬角色: %s"), *VictimName));

				if (APlayerController* PC = Cast<APlayerController>(Victim->GetController()))
				{
					if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
					{
						// 记录当前子系统里还残留多少个 IMC
						TArray<UInputMappingContext*> ActiveIMCs;
						// UE5 较新版本获取上下文方式，直接强转或使用自带方法
						UE_LOG(LogTemp, Warning, TEXT(">>> [卸载清扫] 子系统正常获取！该玩家目前拥有增强输入权限。"));
					}
					else
					{
						UE_LOG(LogTemp, Error, TEXT(">>> [卸载清扫] 致命警告：获取不到 LocalPlayer 的输入子系统！"));
					}

					if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(Victim->InputComponent))
					{
						EIC->ClearActionBindings();
						UE_LOG(LogTemp, Warning, TEXT(">>> [卸载清扫] 已成功调用 EIC->ClearActionBindings()"));
					}
					else
					{
						UE_LOG(LogTemp, Error, TEXT(">>> [卸载清扫] 致命警告：角色 %s 身上没有 EnhancedInputComponent！"), *VictimName);
					}
					PC->FlushPressedKeys();
				}

				GM->FriendlyRoster.RemoveAt(i);
			}
		}
	}
}

void UMyMapTravelSubsystem::PreheatZoneBackground(const UDataLayerAsset* ArtLayerAsset)
{
	if (CachedDataLayerManager.IsValid() && ArtLayerAsset)
	{
		CachedDataLayerManager->SetDataLayerRuntimeState(ArtLayerAsset, EDataLayerRuntimeState::Loaded);
	}
}

void UMyMapTravelSubsystem::ActivateZoneGameplay(const UDataLayerAsset* GameplayLayerAsset, const UDataLayerAsset* ArtLayerAsset)
{
	if (CachedDataLayerManager.IsValid())
	{
		if (ArtLayerAsset)
		{
			CachedDataLayerManager->SetDataLayerRuntimeState(ArtLayerAsset, EDataLayerRuntimeState::Activated);
		}
		if (GameplayLayerAsset)
		{
			CachedDataLayerManager->SetDataLayerRuntimeState(GameplayLayerAsset, EDataLayerRuntimeState::Activated);
		}
	}
}

void UMyMapTravelSubsystem::EliminateZone(const UDataLayerAsset* LayerToUnload)
{
	if (CachedDataLayerManager.IsValid() && LayerToUnload)
	{
		CachedDataLayerManager->SetDataLayerRuntimeState(LayerToUnload, EDataLayerRuntimeState::Unloaded);
	}
}

void UMyMapTravelSubsystem::UpdateEnvironment(UMyBiomeConfig* NewBiome, ADirectionalLight* MainLight, AExponentialHeightFog* MainFog)
{
	if (!NewBiome || !MainLight || !MainLight->GetComponent())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World) return;

	if (CurrentBiomeTarget && CurrentBiomeTarget->BiomeSoundMix)
	{
		UGameplayStatics::PopSoundMixModifier(World, CurrentBiomeTarget->BiomeSoundMix);
	}
	if (NewBiome->BiomeSoundMix)
	{
		UGameplayStatics::PushSoundMixModifier(World, NewBiome->BiomeSoundMix);
	}

	CurrentBiomeTarget = NewBiome;
	CachedSunLight = MainLight;
	CachedAtmosphereFog = MainFog;
	LerpAlpha = 0.0f;

	float Duration = FMath::Max(0.1f, NewBiome->TransitionDuration);
	LerpStep = 0.05f / Duration;

	StartSunRotation = MainLight->GetComponent()->GetComponentRotation();
	StartSunColor = MainLight->GetComponent()->LightColor;

	World->GetTimerManager().SetTimer(BiomeLerpTimer, this, &UMyMapTravelSubsystem::ProcessBiomeLerpTick, 0.05f, true);
}

void UMyMapTravelSubsystem::DebugPrintDataLayerStates()
{
	UDataLayerManager* DLManager = UDataLayerManager::GetDataLayerManager(GetWorld());
	if (!DLManager) return;

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Yellow, TEXT("========== 数据层真实内存状态诊断 =========="));
	}

	for (int32 i = ZoneSequence.Num() - 1; i >= 0; --i)
	{
		const FZoneDataLayerPair& Zone = ZoneSequence[i];

		// 检查玩法层 (Gameplay)
		if (Zone.GameplayLayer)
		{
			EDataLayerRuntimeState GPState = EDataLayerRuntimeState::Unloaded;
			// 【正确的 UE 5.8 写法】：先获取 Instance，再查 State
			if (const UDataLayerInstance* GPInstance = DLManager->GetDataLayerInstance(Zone.GameplayLayer))
			{
				GPState = GPInstance->GetRuntimeState();
			}

			FString StateStr = (GPState == EDataLayerRuntimeState::Activated) ? TEXT("已激活 (Activated)") :
				(GPState == EDataLayerRuntimeState::Loaded) ? TEXT("仅加载 (Loaded)") : TEXT("已卸载 (Unloaded)");

			FColor MsgColor = (GPState == EDataLayerRuntimeState::Activated) ? FColor::Red : FColor::White;
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.f, MsgColor, FString::Printf(TEXT("Zone %d [玩法 Gameplay]: %s"), i, *StateStr));
		}

		// 检查美术层 (Art)
		if (Zone.ArtLayer)
		{
			EDataLayerRuntimeState ArtState = EDataLayerRuntimeState::Unloaded;
			// 【正确的 UE 5.8 写法】：先获取 Instance，再查 State
			if (const UDataLayerInstance* ArtInstance = DLManager->GetDataLayerInstance(Zone.ArtLayer))
			{
				ArtState = ArtInstance->GetRuntimeState();
			}

			FString StateStr = (ArtState == EDataLayerRuntimeState::Activated) ? TEXT("已激活 (Activated)") :
				(ArtState == EDataLayerRuntimeState::Loaded) ? TEXT("仅加载 (Loaded)") : TEXT("已卸载 (Unloaded)");

			FColor MsgColor = (ArtState == EDataLayerRuntimeState::Activated) ? FColor::Green : FColor::White;
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.f, MsgColor, FString::Printf(TEXT("Zone %d [美术 Art]:      %s"), i, *StateStr));
		}
	}
}

void UMyMapTravelSubsystem::ProcessBiomeLerpTick()
{
	if (!CurrentBiomeTarget || !CachedSunLight.IsValid() || !CachedSunLight->GetComponent())
	{
		if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(BiomeLerpTimer);
		return;
	}

	LerpAlpha += LerpStep;
	LerpAlpha = FMath::Clamp(LerpAlpha, 0.0f, 1.0f);

	UDirectionalLightComponent* LightComp = CachedSunLight->GetComponent();

	FRotator NewRot = FMath::Lerp(StartSunRotation, CurrentBiomeTarget->TargetSunRotation, LerpAlpha);
	FLinearColor NewColor = FMath::Lerp(StartSunColor, CurrentBiomeTarget->SunLightColor, LerpAlpha);

	LightComp->SetWorldRotation(NewRot);
	LightComp->SetLightColor(NewColor);

	if (CachedAtmosphereFog.IsValid() && CachedAtmosphereFog->GetComponent())
	{
		CachedAtmosphereFog->GetComponent()->SetFogDensity(
			FMath::Lerp(CachedAtmosphereFog->GetComponent()->FogDensity, CurrentBiomeTarget->FogDensity, LerpAlpha)
		);
	}

	if (LerpAlpha >= 1.0f)
	{
		if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(BiomeLerpTimer);
	}
}

#pragma endregion
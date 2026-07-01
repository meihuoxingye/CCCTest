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
#include "MapTravel/MyTravelSessionSubsystem.h"

#include "Widgets/SOverlay.h"
#include "Widgets/Colors/SColorBlock.h"

#include "Game/MyGameInstance.h"


// ==============================================================================
// 内部工具函数 (Internal Utilities)
// ==============================================================================
#pragma region

APlayerController* UMyMapTravelSubsystem::GetRealPlayerController(UWorld* World) const
{
	// 放弃不稳定的迭代器，直接向生命周期最长、绝不销毁的 GameInstance 索要本地真身
	if (World && World->GetGameInstance())
	{
		return World->GetGameInstance()->GetFirstLocalPlayerController();
	}
	return nullptr;
}

#pragma endregion


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
		// 【实用修复 1】：一键清理当前管家身上的所有定时器（天气过渡、硬切延迟等），根除由于地图销毁引发的空指针闪退
		World->GetTimerManager().ClearAllTimersForObject(this);
	}

	Super::Deinitialize();
}

#pragma endregion


// ==============================================================================
// 核心跳转管线 (Core Travel Pipeline)
// ==============================================================================
#pragma region

void UMyMapTravelSubsystem::ExecuteMapTravel(FName TargetLevelName, TSoftClassPtr<class UUserWidget> CustomLoadingUI, float MinLoadingTime)
{
	if (bIsTraveling) return;

	// ==============================================================================
	// [硬核雷达 1]：起飞前状态抓取（查子弹时间）
	// ==============================================================================
	if (UWorld* ValidWorld = GetWorld())
	{
		float GlobalTD = UGameplayStatics::GetGlobalTimeDilation(ValidWorld);
		float PawnTD = 1.0f;
		if (APlayerController* PC = GetRealPlayerController(ValidWorld))
		{
			if (APawn* Pawn = PC->GetPawn()) PawnTD = Pawn->CustomTimeDilation;
		}

		FString Msg = FString::Printf(TEXT("[雷达-起飞] 去往: %s | 当前全局时间流速: %f | 玩家时间流速: %f"),
			*TargetLevelName.ToString(), GlobalTD, PawnTD);
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Yellow, Msg);
		UE_LOG(LogTemp, Warning, TEXT("================================================="));
		UE_LOG(LogTemp, Warning, TEXT("%s"), *Msg);
	}

	bIsTraveling = true;

	UWorld* World = GetWorld();
	if (!IsValid(World) || TargetLevelName.IsNone())
	{
		bIsTraveling = false;
		return;
	}

	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->SetIgnoreInput(true);
		FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::Cleared);
		FSlateApplication::Get().ReleaseAllPointerCapture();
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			World->GetTimerManager().ClearAllTimersForObject(PC);
			TArray<UActorComponent*> UIComponents;
			PC->GetComponents(UIComponents);
			for (UActorComponent* Comp : UIComponents)
			{
				World->GetTimerManager().ClearAllTimersForObject(Comp);
			}

			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
			{
				Subsystem->ClearAllMappings();
			}

			PC->FlushPressedKeys();
			PC->DisableInput(PC);
			PC->SetIgnoreMoveInput(true);
			PC->SetIgnoreLookInput(true);

			if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PC->InputComponent))
			{
				EIC->ClearActionBindings();
			}

			if (APawn* PlayerPawn = PC->GetPawn())
			{
				PlayerPawn->SetActorEnableCollision(false);

				if (UEnhancedInputComponent* PawnEIC = Cast<UEnhancedInputComponent>(PlayerPawn->InputComponent))
				{
					PawnEIC->ClearActionBindings();
				}
				PC->UnPossess();
			}

			if (PC->GetLevel() != World->PersistentLevel)
			{
				PC->Rename(nullptr, World->PersistentLevel);
			}
			if (PC->PlayerState && PC->PlayerState->GetLevel() != World->PersistentLevel)
			{
				PC->PlayerState->Rename(nullptr, World->PersistentLevel);
			}
		}
	}

	if (UGameInstance* GI = World->GetGameInstance())
	{
		if (UMyTravelSessionSubsystem* TravelSession = GI->GetSubsystem<UMyTravelSessionSubsystem>())
		{
			TravelSession->TargetMapName = TargetLevelName;
			TravelSession->MinimumLoadingTime = MinLoadingTime;
			TravelSession->TravelStartTime = FPlatformTime::Seconds();
		}
	}

	// ==============================================================================
	// 【单点真相】：抛弃原先暂存 Session 并于落地重复构建的冗余思维。
	// 直接把当前物理门所配置的专属过场 UI 动态推往长生避难所，启动物理抗清洗绘制。
	// ==============================================================================
	if (UMyGameInstance* MyGI = Cast<UMyGameInstance>(World->GetGameInstance()))
	{
		MyGI->ShowGlobalBlackScreen(CustomLoadingUI);
	}

	World->ServerTravel(TargetLevelName.ToString());
}

#pragma endregion


// ==============================================================================
// 目标世界到达与强制等待 (Arrival & Artificial Wait)
// ==============================================================================
#pragma region

void UMyMapTravelSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// ==============================================================================
	// [硬核雷达 2]：World 降落时序抓取 (查 UI 闪烁)
	// ==============================================================================
	FString MapName = InWorld.GetMapName();
	FString WorldTypeStr = (InWorld.WorldType == EWorldType::Game) ? TEXT("Game(正式游戏)") :
		(InWorld.WorldType == EWorldType::Inactive) ? TEXT("Inactive(休眠/过渡)") :
		(InWorld.WorldType == EWorldType::PIE) ? TEXT("PIE(编辑器)") : TEXT("Other(其他)");

	FString Msg = FString::Printf(TEXT("[雷达-落地钩子] 触发! 物理地图名: %s | 世界类型: %s"),
		*MapName, *WorldTypeStr);

	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Red, Msg);
	UE_LOG(LogTemp, Error, TEXT("%s"), *Msg);

	UGameInstance* GI = InWorld.GetGameInstance();
	if (!GI) return;

	UMyTravelSessionSubsystem* TravelSession = GI->GetSubsystem<UMyTravelSessionSubsystem>();
	if (!TravelSession) return;

	FString CurrentMapName = InWorld.GetMapName();
	if (TravelSession->TargetMapName.IsNone() || !CurrentMapName.Contains(TravelSession->TargetMapName.ToString()))
	{
		return;
	}

	double ElapsedTime = FPlatformTime::Seconds() - TravelSession->TravelStartTime;
	float RemainingTime = FMath::Max(0.1f, TravelSession->MinimumLoadingTime - static_cast<float>(ElapsedTime));

	// 【清理干净】：由于抽离了灵魂的 Slate 遮罩依然正在不受外界干扰地独立绘制，此处绝不需要二次创建任何视口 UMG 
	if (APlayerController* PC = GetRealPlayerController(&InWorld))
	{
		PC->DisableInput(PC);

		FInputModeUIOnly InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
	}

	InWorld.GetTimerManager().SetTimer(ArrivalTimerHandle, this, &UMyMapTravelSubsystem::FinishMapTravel, RemainingTime, false);
}

void UMyMapTravelSubsystem::FinishMapTravel()
{
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->SetIgnoreInput(false);
	}

	if (UWorld* World = GetWorld())
	{
		// ==============================================================================
		// 【单轨终点】：直接命令不灭的 GameInstance 彻底拆卸掉顶层的物理级 PreLoad 屏障
		// ==============================================================================
		if (UMyGameInstance* MyGI = Cast<UMyGameInstance>(World->GetGameInstance()))
		{
			MyGI->HideGlobalBlackScreen();
		}

		APlayerController* RealPC = GetRealPlayerController(World);

		if (RealPC)
		{
			RealPC->EnableInput(RealPC);

			// [实用修复 3]：杀掉“吞输入/幽灵点击” Bug。强制清空玩家在黑屏期间狂点的残余按键。
			RealPC->FlushPressedKeys();

			FInputModeGameAndUI InputMode;
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			InputMode.SetHideCursorDuringCapture(false);
			RealPC->SetInputMode(InputMode);

			if (APawn* Pawn = RealPC->GetPawn())
			{
				Pawn->EnableInput(RealPC);
			}

			FSlateApplication::Get().SetAllUserFocusToGameViewport();
		}

		// [物理超度]：猎杀无缝传送残留的“幽灵控制器” (修复子弹时间被中和的问题)
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (PC && PC != RealPC)
			{
				if (PC->IsLocalController() || PC->GetLocalPlayer() == nullptr)
				{
					PC->Destroy();
				}
			}
		}
	}

	// ==============================================================================
	// [硬核雷达 3]：落地解禁后状态抓取（对比子弹时间）
	// ==============================================================================
	if (UWorld* ValidWorld = GetWorld())
	{
		float GlobalTD = UGameplayStatics::GetGlobalTimeDilation(ValidWorld);
		float PawnTD = 1.0f;
		if (APlayerController* PC = GetRealPlayerController(ValidWorld))
		{
			if (APawn* Pawn = PC->GetPawn()) PawnTD = Pawn->CustomTimeDilation;
		}

		FString Msg = FString::Printf(TEXT("[雷达-完全解禁] 当前全局时间流速: %f | 玩家时间流速: %f"),
			GlobalTD, PawnTD);
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Green, Msg);
		UE_LOG(LogTemp, Warning, TEXT("%s"), *Msg);
		UE_LOG(LogTemp, Warning, TEXT("================================================="));
	}

	bIsTraveling = false;
}

#pragma endregion


// ==============================================================================
// 同地图硬切换管线 (Intra-Map Hard Travel)
// ==============================================================================
#pragma region

void UMyMapTravelSubsystem::ExecuteZoneTravelWithWait(UDataLayerAsset* TargetZone, TSoftClassPtr<class UUserWidget> CustomLoadingUI, float WaitTime)
{
	if (bIsTraveling || !TargetZone) return;
	bIsTraveling = true;

	UWorld* World = GetWorld();
	if (!World)
	{
		bIsTraveling = false;
		return;
	}

	if (APlayerController* PC = GetRealPlayerController(World))
	{
		// ==============================================================================
		// 【单点真相】：直接把函数接收到的 UI 扔给 GameInstance
		// ==============================================================================
		if (UMyGameInstance* MyGI = Cast<UMyGameInstance>(World->GetGameInstance()))
		{
			MyGI->ShowGlobalBlackScreen(CustomLoadingUI);
		}

		PC->DisableInput(PC);
		FInputModeUIOnly InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);

		if (APawn* Pawn = PC->GetPawn())
		{
			Pawn->DisableInput(PC);
		}
	}

	RefreshSlidingWindow(TargetZone);

	World->GetTimerManager().SetTimer(ZoneTravelTimerHandle, this, &UMyMapTravelSubsystem::FinishZoneTravel, WaitTime, false);
}

void UMyMapTravelSubsystem::FinishZoneTravel()
{
	if (ZoneLoadingWidget)
	{
		ZoneLoadingWidget->RemoveFromParent();
		ZoneLoadingWidget = nullptr;
	}

	if (UWorld* World = GetWorld())
	{
		// 【架构升级】：呼叫 GameInstance 砸碎防弹玻璃
		if (UMyGameInstance* MyGI = Cast<UMyGameInstance>(World->GetGameInstance()))
		{
			MyGI->HideGlobalBlackScreen();
		}

		if (APlayerController* PC = GetRealPlayerController(World))
		{
			PC->EnableInput(PC);
			PC->FlushPressedKeys();

			FInputModeGameAndUI InputMode;
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			InputMode.SetHideCursorDuringCapture(false);
			PC->SetInputMode(InputMode);

			if (APawn* Pawn = PC->GetPawn())
			{
				Pawn->EnableInput(PC);
			}

			FSlateApplication::Get().SetAllUserFocusToGameViewport();
		}
	}

	bIsTraveling = false;
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
	UDataLayerManager* DLManager = UDataLayerManager::GetDataLayerManager(GetWorld());
	if (!DLManager || ZoneSequence.Num() == 0 || !TriggeredLayer) return;

	if (LastActiveZone == TriggeredLayer) return;
	LastActiveZone = TriggeredLayer;

	int32 CurrentIdx = INDEX_NONE;
	for (int32 i = 0; i < ZoneSequence.Num(); ++i)
	{
		if (ZoneSequence[i].ArtLayer == TriggeredLayer || ZoneSequence[i].GameplayLayer == TriggeredLayer)
		{
			CurrentIdx = i;
			break;
		}
	}

	if (CurrentIdx == INDEX_NONE) return;

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
			if (Zone.GameplayLayer) DLManager->SetDataLayerRuntimeState(Zone.GameplayLayer, EDataLayerRuntimeState::Unloaded);
		}
		else
		{
			if (Zone.ArtLayer) DLManager->SetDataLayerRuntimeState(Zone.ArtLayer, EDataLayerRuntimeState::Unloaded);
			if (Zone.GameplayLayer) DLManager->SetDataLayerRuntimeState(Zone.GameplayLayer, EDataLayerRuntimeState::Unloaded);
		}
	}

	if (GEngine && GetWorld())
	{
		GEngine->Exec(GetWorld(), TEXT("r.TextureStreaming.ForceUpdate"));
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
	if (!NewBiome || !MainLight || !MainLight->GetComponent()) return;
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

		if (Zone.GameplayLayer)
		{
			EDataLayerRuntimeState GPState = EDataLayerRuntimeState::Unloaded;
			if (const UDataLayerInstance* GPInstance = DLManager->GetDataLayerInstance(Zone.GameplayLayer))
			{
				GPState = GPInstance->GetRuntimeState();
			}

			FString StateStr = (GPState == EDataLayerRuntimeState::Activated) ? TEXT("已激活 (Activated)") :
				(GPState == EDataLayerRuntimeState::Loaded) ? TEXT("仅加载 (Loaded)") : TEXT("已卸载 (Unloaded)");

			FColor MsgColor = (GPState == EDataLayerRuntimeState::Activated) ? FColor::Red : FColor::White;
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.f, MsgColor, FString::Printf(TEXT("Zone %d [玩法 Gameplay]: %s"), i, *StateStr));
		}

		if (Zone.ArtLayer)
		{
			EDataLayerRuntimeState ArtState = EDataLayerRuntimeState::Unloaded;
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
	if (!IsValid(GetWorld()) || !CurrentBiomeTarget || !CachedSunLight.IsValid() || !CachedSunLight->GetComponent())
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
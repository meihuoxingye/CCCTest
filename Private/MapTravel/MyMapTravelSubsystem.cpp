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
#include "Game/MyGameViewportClient.h"

// ==============================================================================
// 内部安全获取真实玩家控制器的工具函数 (防无缝传送假身)
// ==============================================================================
namespace
{
	APlayerController* GetRealPlayerController(UWorld* World)
	{
		if (!World) return nullptr;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (PC && PC->IsLocalController() && PC->GetLocalPlayer())
			{
				return PC;
			}
		}
		return nullptr;
	}
}

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
		World->GetTimerManager().ClearAllTimersForObject(this);
	}

	Super::Deinitialize();
}

void UMyMapTravelSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	FString CurrentMapName = InWorld.GetMapName();

	if (UMyGameInstance* GI = InWorld.GetGameInstance<UMyGameInstance>())
	{
		FString CleanTargetMap = FPackageName::GetShortName(GI->PendingTargetMapName.ToString());
		FString CleanCurrentMap = FPackageName::GetShortName(CurrentMapName);

		// 过滤过渡地图
		if (GI->PendingTargetMapName.IsNone() || !CleanCurrentMap.Contains(CleanTargetMap))
		{
			return;
		}

		/*
		// 【真正落地】：新世界加载完成！
		// 此时 GameInstance 监听到 PostLoadMapWithWorld，底层的 BlackoutExtension 断路器已自动解除。
		// 我们顺势在屏幕上贴好精美的 UMG 加载图，完美衔接！
		GI->ShowFakeLoadingScreen(nullptr);
		*/
	}

	// 恢复玩家控制权
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->SetIgnoreInput(false);
	}

	if (APlayerController* PC = GetRealPlayerController(&InWorld))
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
			Pawn->SetActorEnableCollision(true);
		}

		FSlateApplication::Get().SetAllUserFocusToGameViewport();
	}

	bIsTraveling = false;
}

#pragma endregion


// ==============================================================================
// 核心跳转管线 (Core Travel Pipeline)
// ==============================================================================
#pragma region

void UMyMapTravelSubsystem::ExecuteMapTravel(FName TargetLevelName, TSoftClassPtr<class UUserWidget> CustomLoadingUI, float MinLoadingTime)
{
	if (bIsTraveling) return;
	bIsTraveling = true;

	UWorld* World = GetWorld();
	if (!IsValid(World) || TargetLevelName.IsNone())
	{
		bIsTraveling = false;
		return;
	}

	// 1. 瞬间剥夺控制权
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

			/*待修改*/
			if (PC->GetLevel() != World->PersistentLevel)
			{
				PC->Rename(nullptr, World->PersistentLevel);
			}
			if (PC->PlayerState && PC->PlayerState->GetLevel() != World->PersistentLevel)
			{
				PC->PlayerState->Rename(nullptr, World->PersistentLevel);
			}
			/*待修改*/
		}
	}

	// 2. 记忆目标地图，并呼叫普通的 Slate 渐暗 UI
	if (UMyGameInstance* GI = World->GetGameInstance<UMyGameInstance>())
	{
		GI->PendingTargetMapName = TargetLevelName;

		if (!CustomLoadingUI.IsNull())
		{
			GI->DefaultLoadingUIClass = CustomLoadingUI;
		}

		// 触发 0.3 秒 Slate 渐暗，让黑幕平滑过渡
		GI->StartBlackFade();
	}

	// 3. 等待 0.35 秒让 Slate 黑透，然后直接执行无缝漫游！
	FTimerHandle TravelTimerHandle;
	World->GetTimerManager().SetTimer(TravelTimerHandle, [World, TargetLevelName]()
		{
			if (IsValid(World))
			{
				// 【终极闭环】：只要这句话一执行，引擎广播 OnSeamlessTravelTransition。
				// GameInstance 监听到后瞬间激活 BlackoutExtension。
				// 渲染管线立刻断电清黑，光追 TLAS 停止更新，完美跨越垃圾回收与地图流送！
				World->ServerTravel(TargetLevelName.ToString());
			}
		}, 0.35f, false);
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
		PC->DisableInput(PC);
		FInputModeUIOnly InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);

		if (APawn* Pawn = PC->GetPawn())
		{
			Pawn->DisableInput(PC);
		}

		if (!CustomLoadingUI.IsNull())
		{
			if (UClass* LoadedClass = CustomLoadingUI.LoadSynchronous())
			{
				/*待修改*/
				ZoneLoadingWidget = CreateWidget<UUserWidget>(PC, LoadedClass);
				if (ZoneLoadingWidget)
				{
					ZoneLoadingWidget->AddToViewport(9999);
				}
				/*待修改*/
			}
		}
	}

	RefreshSlidingWindow(TargetZone);

	World->GetTimerManager().SetTimer(ZoneTravelTimerHandle, this, &UMyMapTravelSubsystem::FinishZoneTravel, WaitTime, false);
}

void UMyMapTravelSubsystem::FinishZoneTravel()
{
	/*待修改*/
	if (ZoneLoadingWidget)
	{
		ZoneLoadingWidget->RemoveFromParent();
		ZoneLoadingWidget = nullptr;
	}
	/*待修改*/

	if (UWorld* World = GetWorld())
	{
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

/*
// ==============================================================================
// 硬核雷达监控 (Hardcore Radar)
// ==============================================================================
#pragma region

void UMyMapTravelSubsystem::HardcoreRadarTick()
{
	UWorld* World = GetWorld();
	if (!World || World->bIsTearingDown)
	{
		return;
	}

	APlayerController* PC = nullptr;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* TempPC = It->Get();
		if (IsValid(TempPC) && TempPC->Player != nullptr)
		{
			PC = TempPC;
			break;
		}
	}

	/*
	FString CamMsg = TEXT("Cam: 无效");
	if (PC && PC->PlayerCameraManager)
	{
		FVector Loc = PC->PlayerCameraManager->GetCameraLocation();
		CamMsg = FString::Printf(TEXT("Cam: %.0f,%.0f"), Loc.X, Loc.Y);
	}
}

#pragma endregion
*/
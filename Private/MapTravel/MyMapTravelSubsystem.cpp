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
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "Framework/Application/SlateApplication.h" 
#include "Engine/GameViewportClient.h"
#include "Character/TopCharacter.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/PlayerState.h"
#include "Blueprint/UserWidget.h"
#include "MapTravel/MyTravelSessionSubsystem.h"

#include "Widgets/Layout/SSpacer.h"
#include "MoviePlayer.h"


// ==============================================================================
// 内部工具函数
// ==============================================================================
#pragma region

APlayerController* UMyMapTravelSubsystem::GetRealPlayerController(UWorld* World) const
{
	if (World && World->GetGameInstance())
	{
		return World->GetGameInstance()->GetFirstLocalPlayerController();
	}
	return nullptr;
}

#pragma endregion


// ==============================================================================
// 生命周期与初始化
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

#pragma endregion


// ==============================================================================
// 核心跳转管线
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

	// 1. 【仅隔离输入，绝不致盲】：此时绝不拉断渲染，防止起飞瞬间变黑
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->SetIgnoreInput(true);
	}

	FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::Cleared);
	FSlateApplication::Get().ReleaseAllPointerCapture();

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			World->GetTimerManager().ClearAllTimersForObject(PC);
			TArray<UActorComponent*> UIComponents;
			PC->GetComponents(UIComponents);
			for (UActorComponent* Comp : UIComponents) World->GetTimerManager().ClearAllTimersForObject(Comp);

			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer())) Subsystem->ClearAllMappings();

			PC->FlushPressedKeys();
			PC->DisableInput(PC);
			PC->SetIgnoreMoveInput(true);
			PC->SetIgnoreLookInput(true);

			if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PC->InputComponent)) EIC->ClearActionBindings();
			if (APawn* PlayerPawn = PC->GetPawn())
			{
				PlayerPawn->SetActorEnableCollision(false);
				if (UEnhancedInputComponent* PawnEIC = Cast<UEnhancedInputComponent>(PlayerPawn->InputComponent)) PawnEIC->ClearActionBindings();
				PC->UnPossess();
			}

			if (PC->GetLevel() != World->PersistentLevel) PC->Rename(nullptr, World->PersistentLevel);
			if (PC->PlayerState && PC->PlayerState->GetLevel() != World->PersistentLevel) PC->PlayerState->Rename(nullptr, World->PersistentLevel);
		}
	}

	// 2. 【神级欺骗】：关闭正在运行的默认黑屏，塞入一个透明占位符堵住引擎的嘴！绝不调用 PlayMovie！
	if (GetMoviePlayer() != nullptr)
	{
		GetMoviePlayer()->StopMovie();

		FLoadingScreenAttributes DummyScreen;
		DummyScreen.bAutoCompleteWhenLoadingCompletes = true;
		DummyScreen.WidgetLoadingScreen = SNew(SSpacer); // 透明占位，废掉保底黑屏
		GetMoviePlayer()->SetupLoadingScreen(DummyScreen);
	}

	// 3. 【0帧延迟注入】：把真实 UI 挂载到绝对同步的 GameViewport
	if (UGameInstance* GI = World->GetGameInstance())
	{
		if (UMyTravelSessionSubsystem* TravelSession = GI->GetSubsystem<UMyTravelSessionSubsystem>())
		{
			TravelSession->PendingLoadingWidgetClass = CustomLoadingUI;
			TravelSession->TargetMapName = TargetLevelName;
			TravelSession->MinimumLoadingTime = MinLoadingTime;
			TravelSession->TravelStartTime = FPlatformTime::Seconds();

			if (!CustomLoadingUI.IsNull())
			{
				if (UClass* LoadedClass = CustomLoadingUI.LoadSynchronous())
				{
					// 以 GameInstance 为父对象创建，保证旧 World 毁灭时 UI 绝不暴毙
					TravelSession->CrossLevelLoadingWidget = CreateWidget<UUserWidget>(GI, LoadedClass);
					if (TravelSession->CrossLevelLoadingWidget)
					{
						TravelSession->CrossLevelLoadingWidget->AddToRoot(); // 赋予免死金牌
						TravelSession->CrossLevelSafeWidget = TravelSession->CrossLevelLoadingWidget->TakeWidget(); // 脱壳

						// 挂载到 GameViewport，同线程渲染，绝无起飞黑屏差！
						if (GEngine && GEngine->GameViewport && TravelSession->CrossLevelSafeWidget.IsValid())
						{
							GEngine->GameViewport->AddViewportWidgetContent(TravelSession->CrossLevelSafeWidget.ToSharedRef(), 10000);
						}
					}
				}
			}
		}
	}

	// 4. 【精准致盲】：此时 UI 已经无延迟上屏，立刻切断 3D 渲染，彻底抹杀后续的 (0,0,0) 漏光！
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->bDisableWorldRendering = true;
	}

	World->ServerTravel(TargetLevelName.ToString());
}

#pragma endregion


// ==============================================================================
// 目标世界到达与强制等待
// ==============================================================================
#pragma region

void UMyMapTravelSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// 确保新世界落地时依然处于致盲状态
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->bDisableWorldRendering = true;
	}

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

	if (APlayerController* PC = GetRealPlayerController(&InWorld))
	{
		PC->DisableInput(PC);
	}

	InWorld.GetTimerManager().SetTimer(ArrivalTimerHandle, this, &UMyMapTravelSubsystem::FinishMapTravel, RemainingTime, false);
}

void UMyMapTravelSubsystem::FinishMapTravel()
{
	UWorld* World = GetWorld();
	if (!World) return;

	APlayerController* RealPC = GetRealPlayerController(World);

	// 1. 【斩断残影】：在撤除 UI 前，强制修正相机
	if (RealPC && RealPC->PlayerCameraManager)
	{
		RealPC->PlayerCameraManager->bGameCameraCutThisFrame = true;
		RealPC->PlayerCameraManager->UpdateCamera(0.0f);
	}

	// 2. 【清理假黑屏】：确保关闭我们之前设置的透明 DummyScreen
	if (GetMoviePlayer() != nullptr)
	{
		GetMoviePlayer()->StopMovie();
	}

	// 3. 【安全拆除】：从 GameViewport 中卸载跨界 UI
	if (UGameInstance* GI = World->GetGameInstance())
	{
		if (UMyTravelSessionSubsystem* TravelSession = GI->GetSubsystem<UMyTravelSessionSubsystem>())
		{
			// 因为我们在起飞时把它挂在了 GameViewport 上，现在必须在这里安全移除
			if (GEngine && GEngine->GameViewport && TravelSession->CrossLevelSafeWidget.IsValid())
			{
				GEngine->GameViewport->RemoveViewportWidgetContent(TravelSession->CrossLevelSafeWidget.ToSharedRef());
			}

			if (TravelSession->CrossLevelLoadingWidget)
			{
				TravelSession->CrossLevelLoadingWidget->RemoveFromRoot();
			}
			TravelSession->CrossLevelSafeWidget.Reset();
			TravelSession->CrossLevelLoadingWidget = nullptr;
		}
	}

	// 4. 【恢复视觉与输入】：3D 世界重见天日
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->bDisableWorldRendering = false;
		GEngine->GameViewport->SetIgnoreInput(false);
	}

	if (RealPC)
	{
		RealPC->EnableInput(RealPC);
		RealPC->FlushPressedKeys();

		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		RealPC->SetInputMode(InputMode);

		if (APawn* Pawn = RealPC->GetPawn()) Pawn->EnableInput(RealPC);
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && PC != RealPC)
		{
			if (PC->IsLocalController() || PC->GetLocalPlayer() == nullptr) PC->Destroy();
		}
	}

	bIsTraveling = false;
}

#pragma endregion


// ==============================================================================
// 同地图硬切换管线
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
		if (GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->bDisableWorldRendering = true;
		}

		PC->DisableInput(PC);
		FInputModeUIOnly InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
		if (APawn* Pawn = PC->GetPawn()) Pawn->DisableInput(PC);

		if (!CustomLoadingUI.IsNull())
		{
			if (UClass* LoadedClass = CustomLoadingUI.LoadSynchronous())
			{
				ZoneLoadingWidget = CreateWidget<UUserWidget>(World, LoadedClass);
				if (ZoneLoadingWidget)
				{
					ZoneLoadingWidget->AddToRoot();
					ZoneSlateWidget = ZoneLoadingWidget->TakeWidget();
					if (GEngine && GEngine->GameViewport && ZoneSlateWidget.IsValid())
					{
						GEngine->GameViewport->AddViewportWidgetContent(ZoneSlateWidget.ToSharedRef(), 10000);
					}
				}
			}
		}
	}

	RefreshSlidingWindow(TargetZone);
	World->GetTimerManager().SetTimer(ZoneTravelTimerHandle, this, &UMyMapTravelSubsystem::FinishZoneTravel, WaitTime, false);
}

void UMyMapTravelSubsystem::FinishZoneTravel()
{
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = GetRealPlayerController(World))
		{
			if (PC->PlayerCameraManager)
			{
				PC->PlayerCameraManager->bGameCameraCutThisFrame = true;
				PC->PlayerCameraManager->UpdateCamera(0.0f);
			}

			if (GEngine && GEngine->GameViewport)
			{
				if (ZoneSlateWidget.IsValid())
				{
					GEngine->GameViewport->RemoveViewportWidgetContent(ZoneSlateWidget.ToSharedRef());
					ZoneSlateWidget.Reset();
				}
				GEngine->GameViewport->bDisableWorldRendering = false;
			}

			if (ZoneLoadingWidget)
			{
				ZoneLoadingWidget->RemoveFromRoot();
				ZoneLoadingWidget = nullptr;
			}

			PC->EnableInput(PC);
			PC->FlushPressedKeys();

			FInputModeGameAndUI InputMode;
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			InputMode.SetHideCursorDuringCapture(false);
			PC->SetInputMode(InputMode);

			if (APawn* Pawn = PC->GetPawn()) Pawn->EnableInput(PC);
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
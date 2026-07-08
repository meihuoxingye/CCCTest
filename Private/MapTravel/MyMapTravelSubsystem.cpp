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

// ==============================================================================
// 【新增】：内部安全获取真实玩家控制器的工具函数 (防无缝传送假身)
// ==============================================================================
namespace
{
	APlayerController* GetRealPlayerController(UWorld* World)
	{
		if (!World) return nullptr;
		// 遍历所有控制器，只抓取真正拥有本地玩家 (LocalPlayer) 的“真身”
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
	// 在函数开头加入
	if (FSlateApplication::IsInitialized())
	{
		TArray<TSharedRef<SWindow>> AllWindows;
		FSlateApplication::Get().GetAllVisibleWindowsOrdered(AllWindows);

		UE_LOG(LogTemp, Warning, TEXT("=== 正在探测物理窗口 (共 %d 个) ==="), AllWindows.Num());

		for (int32 i = 0; i < AllWindows.Num(); ++i)
		{
			TSharedRef<SWindow> Win = AllWindows[i];
			FString WinTitle = Win->GetTitle().ToString();
			// 抓取这个窗口里的内容类名
			FString ContentType = Win->GetContent()->GetTypeAsString();

			UE_LOG(LogTemp, Warning, TEXT("窗口 [%d]: 标题='%s', 内容类名='%s'"), i, *WinTitle, *ContentType);

			// 如果你找到了那个带点和文字的窗口，可以用下面这行物理干掉它
			if (ContentType.Contains(TEXT("PreLoad")) || ContentType.Contains(TEXT("Loading")))
			{
				// 临时测试：直接物理隐藏这个挡路的东西
				Win->SetOpacity(0.0f);
				UE_LOG(LogTemp, Error, TEXT("已物理隐藏幽灵窗口: %s"), *ContentType);
			}
		}
	}

	if (bIsTraveling) return;

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

	//---------
	// 强制覆盖外部传入的时间，将新地图到达后的强制黑屏等待时间延长至 15 秒，方便慢慢观察
	MinLoadingTime = 15.0f;
	//---------

	if (UGameInstance* GI = World->GetGameInstance())
	{
		if (UMyTravelSessionSubsystem* TravelSession = GI->GetSubsystem<UMyTravelSessionSubsystem>())
		{
			TravelSession->PendingLoadingWidgetClass = CustomLoadingUI;
			TravelSession->TargetMapName = TargetLevelName;
			TravelSession->MinimumLoadingTime = MinLoadingTime;
			TravelSession->TravelStartTime = FPlatformTime::Seconds();
		}
	}

	if (!CustomLoadingUI.IsNull())
	{
		if (UClass* LoadedWidgetClass = CustomLoadingUI.LoadSynchronous())
		{
			// 【修改 1】：从 GetFirstPlayerController 换成了 GetRealPlayerController
			if (APlayerController* PC = GetRealPlayerController(World))
			{
				if (UUserWidget* PreLoadingUI = CreateWidget<UUserWidget>(PC, LoadedWidgetClass))
				{
					PreLoadingUI->AddToViewport(9999);
				}
			}
		}
	}

	/*
	World->ServerTravel(TargetLevelName.ToString());
	*/

	//---------
	// 人为插入 5 秒的纯净观察期：UI 此时已经生成，但由于尚未触发 ServerTravel，主线程未卡死，雷达可以全速输出
	UE_LOG(LogTemp, Warning, TEXT("=== [测试延迟] PreLoading UI 已挂载，等待 5 秒后正式触发 ServerTravel... ==="));
	FTimerHandle DelayTravelTimer;
	World->GetTimerManager().SetTimer(DelayTravelTimer, [World, TargetLevelName]()
		{
			if (IsValid(World))
			{
				UE_LOG(LogTemp, Error, TEXT(">>>>> [测试延迟] 5秒已满！正式触发 ServerTravel，主线程即将进入卡死断层区 <<<<<"));
				World->ServerTravel(TargetLevelName.ToString());
			}
		}, 5.0f, false);
	//---------
}

#pragma endregion


// ==============================================================================
// 目标世界到达与强制等待 (Arrival & Artificial Wait)
// ==============================================================================
#pragma region

void UMyMapTravelSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	UGameInstance* GI = InWorld.GetGameInstance();
	if (!GI) return;

	UMyTravelSessionSubsystem* TravelSession = GI->GetSubsystem<UMyTravelSessionSubsystem>();
	if (!TravelSession) return;

	FString CurrentMapName = InWorld.GetMapName();
	if (TravelSession->TargetMapName.IsNone() || !CurrentMapName.Contains(TravelSession->TargetMapName.ToString()))
	{
		return;
	}

	UClass* TargetUIClass = TravelSession->ConsumeLoadingClass();
	if (TargetUIClass)
	{
		// 【修改 2】：从 GetFirstPlayerController 换成了 GetRealPlayerController
		if (APlayerController* PC = GetRealPlayerController(&InWorld))
		{
			PC->DisableInput(PC);

			FInputModeUIOnly InputMode;
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			PC->SetInputMode(InputMode);

			ArrivalLoadingWidget = CreateWidget<UUserWidget>(PC, TargetUIClass);
			if (ArrivalLoadingWidget)
			{
				ArrivalLoadingWidget->AddToViewport(9999);
			}
		}

		double ElapsedTime = FPlatformTime::Seconds() - TravelSession->TravelStartTime;
		float RemainingTime = FMath::Max(0.1f, TravelSession->MinimumLoadingTime - static_cast<float>(ElapsedTime));

		InWorld.GetTimerManager().SetTimer(ArrivalTimerHandle, this, &UMyMapTravelSubsystem::FinishMapTravel, RemainingTime, false);
	}
}

void UMyMapTravelSubsystem::FinishMapTravel()
{
	// 【实用修复 2】：彻底解除引擎视口的物理级死锁，否则鼠标在某些全屏模式下会完全失效
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->SetIgnoreInput(false);
	}

	if (ArrivalLoadingWidget)
	{
		ArrivalLoadingWidget->RemoveFromParent();
		ArrivalLoadingWidget = nullptr;
	}

	if (UWorld* World = GetWorld())
	{
		// 【修改 3】：从 GetFirstPlayerController 换成了 GetRealPlayerController
		if (APlayerController* PC = GetRealPlayerController(World))
		{
			PC->EnableInput(PC);

			// 【实用修复 3】：杀掉“吞输入/幽灵点击” Bug。强制清空玩家在黑屏期间狂点的残余按键。
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

	// 【修改 4】：从 GetFirstPlayerController 换成了 GetRealPlayerController
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
				ZoneLoadingWidget = CreateWidget<UUserWidget>(PC, LoadedClass);
				if (ZoneLoadingWidget)
				{
					ZoneLoadingWidget->AddToViewport(9999);
				}
			}
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
		// 【修改 5】：从 GetFirstPlayerController 换成了 GetRealPlayerController
		if (APlayerController* PC = GetRealPlayerController(World))
		{
			PC->EnableInput(PC);

			// 【同上清理残余输入】
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
	// 增加有效性判断，防止关卡销毁时触发 Tick 导致闪退
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


// ==============================================================================
// 硬核雷达监控 (Hardcore Radar)
// ==============================================================================
#pragma region

void UMyMapTravelSubsystem::HardcoreRadarTick()
{
	UWorld* World = GetWorld();
	// 如果 World 正在销毁或已失效，立即停止探测，防止访问空指针
	if (!World || World->bIsTearingDown)
	{
		return;
	}

	// 安全获取 LocalPlayer 对应的 PC
	APlayerController* PC = nullptr;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* TempPC = It->Get();
		// 关键防护：不仅判空，还要通过 IsValidLowLevel 确保对象没被回收，且 Player 指针有效
		if (IsValid(TempPC) && TempPC->Player != nullptr)
		{
			PC = TempPC;
			break;
		}
	}

	FString UIMsg = TEXT("UI: 丢失");
	if (UGameInstance* GI = World->GetGameInstance())
	{
		if (UMyTravelSessionSubsystem* TS = GI->GetSubsystem<UMyTravelSessionSubsystem>())
		{
			if (TS->CrossLevelSafeWidget.IsValid()) UIMsg = TEXT("UI: 正常");
		}
	}

	FString CamMsg = TEXT("Cam: 无效");
	if (PC && PC->PlayerCameraManager)
	{
		FVector Loc = PC->PlayerCameraManager->GetCameraLocation();
		CamMsg = FString::Printf(TEXT("Cam: %.0f,%.0f"), Loc.X, Loc.Y);
	}

	FString FinalMsg = FString::Printf(TEXT("[雷达] %s | %s | %s"), *World->GetMapName(), *UIMsg, *CamMsg);
	if (GEngine) GEngine->AddOnScreenDebugMessage(19999, 0.06f, FColor::Cyan, FinalMsg);
}

#pragma endregion
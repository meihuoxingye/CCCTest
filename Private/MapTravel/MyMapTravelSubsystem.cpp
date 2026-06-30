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

#include "GameFramework/PlayerState.h" // 必须添加这一行
#include "Blueprint/UserWidget.h"

#include "MapTravel/MyTravelSessionSubsystem.h"


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

void UMyMapTravelSubsystem::ExecuteMapTravel(FName TargetLevelName, TSoftClassPtr<class UUserWidget> CustomLoadingUI)
{
	if (bIsTraveling)
	{
		return;
	}

	// 【核心修复 1】：立刻上锁！
	// Standalone模式下物理步长极易导致同一帧多次碰撞重叠，必须在第一行就封死重入！
	bIsTraveling = true;

	UWorld* World = GetWorld();
	if (!IsValid(World) || TargetLevelName.IsNone())
	{
		bIsTraveling = false; // 如果跳转条件不满足，记得解锁
		return;
	}

	// 1. 制造引擎级输入黑洞 (Engine Viewport Isolation)
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->SetIgnoreInput(true);

		FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::Cleared);
		FSlateApplication::Get().ReleaseAllPointerCapture();
	}

	// 2. 逻辑隔离取代物理拆除 (Isolation vs. Destruction)
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

			// 合法冲刷后，彻底软禁控制器
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

			// ==============================================================================
			// 【终极防线：物理偷渡持久关卡 (Standalone 0x30 闪退救星)】
			// ==============================================================================
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

	// ==============================================================================
	// 把触发器传进来的 UI 存入跨关卡的 Session 中 (不论是有效类还是 nullptr)
	// ==============================================================================
	if (UGameInstance* GI = World->GetGameInstance())
	{
		if (UMyTravelSessionSubsystem* TravelSession = GI->GetSubsystem<UMyTravelSessionSubsystem>())
		{
			TravelSession->PendingLoadingWidgetClass = CustomLoadingUI;

			// 【核心新增】：记录飞机起飞的绝对引擎时间！
			TravelSession->TravelStartTime = FPlatformTime::Seconds();
		}
	}

	// ==============================================================================
	// 【第一棒视觉兜底】：只有在明确传入了 UI 时才遮盖屏幕
	// ==============================================================================
	if (!CustomLoadingUI.IsNull())
	{
		if (UClass* LoadedWidgetClass = CustomLoadingUI.LoadSynchronous())
		{
			if (APlayerController* PC = World->GetFirstPlayerController())
			{
				if (UUserWidget* PreLoadingUI = CreateWidget<UUserWidget>(PC, LoadedWidgetClass))
				{
					PreLoadingUI->AddToViewport(9999);
				}
			}
		}
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

	UGameInstance* GI = InWorld.GetGameInstance();
	if (!GI) return;

	UMyTravelSessionSubsystem* TravelSession = GI->GetSubsystem<UMyTravelSessionSubsystem>();
	if (!TravelSession) return;

	// 1. 尝试从黑匣子拿走 UI（Stage 3 拦截，彻底消费数据）
	UClass* TargetUIClass = TravelSession->ConsumeLoadingClass();
	if (TargetUIClass)
	{
		if (APlayerController* PC = InWorld.GetFirstPlayerController())
		{
			// 2. 双重锁定：禁用输入并强制切换到 UI 模式，彻底消除按键与鼠标穿透
			PC->DisableInput(PC);
			FInputModeUIOnly InputMode;
			PC->SetInputMode(InputMode);

			// 3. 【第三棒拉起】：在新世界继续掩护
			ArrivalLoadingWidget = CreateWidget<UUserWidget>(PC, TargetUIClass);
			if (ArrivalLoadingWidget)
			{
				ArrivalLoadingWidget->AddToViewport(9999);
			}
		}

		// 4. 精准补时：计算从旧世界起飞到现在流逝的真实时间
		double ElapsedTime = FPlatformTime::Seconds() - TravelSession->TravelStartTime;
		float RemainingTime = FMath::Max(0.2f, TravelSession->MinimumLoadingTime - static_cast<float>(ElapsedTime));

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, RemainingTime, FColor::Yellow, FString::Printf(TEXT(">>> [目标世界] 落地过快！强制补足 %.2f 秒最小等待时间..."), RemainingTime));
		}

		// 5. 设置解锁倒计时 (此时有了 UFUNCTION，SetTimer 将完美生效)
		InWorld.GetTimerManager().SetTimer(ArrivalTimerHandle, this, &UMyMapTravelSubsystem::FinishMapTravel, RemainingTime, false);
	}
}

void UMyMapTravelSubsystem::FinishMapTravel()
{
	// 1. 彻底恢复输入焦点与游戏内输入模式
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			PC->EnableInput(PC);
			FInputModeGameOnly InputMode;
			PC->SetInputMode(InputMode);

			if (APawn* Pawn = PC->GetPawn())
			{
				Pawn->EnableInput(PC);
			}
		}
	}

	// 2. 彻底销毁过场 UI，把视野还给玩家
	if (ArrivalLoadingWidget)
	{
		ArrivalLoadingWidget->RemoveFromParent();
		ArrivalLoadingWidget = nullptr;
	}

	bIsTraveling = false;

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT(">>> [目标世界] 过场掩护结束，流送完毕，允许游玩！"));
	}
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

	// 极简状态机调度：不再进行死循环名册校验，交由底层事件总线完成
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

	DebugPrintDataLayerStates();
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
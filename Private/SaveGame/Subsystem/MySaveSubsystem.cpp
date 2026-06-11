// Fill out your copyright notice in the Description page of Project Settings.

#include "SaveGame/Subsystem/MySaveSubsystem.h"
#include "SaveGame/MySaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"

#pragma region

void UMySaveSubsystem::PreloadRegistry()
{
	const FString RegistrySlot = TEXT("GlobalSaveRegistry");
	FAsyncLoadGameFromSlotDelegate LoadedDelegate;
	LoadedDelegate.BindUObject(this, &UMySaveSubsystem::OnRegistryLoaded);

	// 彻底告别主线程阻塞，开启后台异步读取
	UGameplayStatics::AsyncLoadGameFromSlot(RegistrySlot, 0, LoadedDelegate);
}

void UMySaveSubsystem::OnRegistryLoaded(const FString& SlotName, const int32 UserIndex, USaveGame* LoadedGame)
{
	if (LoadedGame)
	{
		CachedRegistry = Cast<UMySaveRegistry>(LoadedGame);
	}
	else
	{
		CachedRegistry = Cast<UMySaveRegistry>(UGameplayStatics::CreateSaveGameObject(UMySaveRegistry::StaticClass()));
	}

	OnSaveRegistryChanged.Broadcast();
}

TArray<FSaveSlotMetaData> UMySaveSubsystem::GetSaveSlotList()
{
	TArray<FSaveSlotMetaData> OutList;
	// 直接 O(1) 读内存缓存，耗时为 0
	if (CachedRegistry)
	{
		for (const auto& Pair : CachedRegistry->SaveSlots)
		{
			OutList.Add(Pair.Value);
		}
		OutList.Sort([](const FSaveSlotMetaData& A, const FSaveSlotMetaData& B) {
			return A.SaveTime > B.SaveTime;
			});
	}
	return OutList;
}

void UMySaveSubsystem::PerformAsyncSave(const FString& SlotName)
{
	UMySaveGame* SaveObj = Cast<UMySaveGame>(UGameplayStatics::CreateSaveGameObject(UMySaveGame::StaticClass()));
	if (!SaveObj)
	{
		OnSaveFinished.Broadcast(false);
		return;
	}

	UWorld* World = GetWorld();
	FName CurrentLevelName = World ? FName(*World->GetName()) : NAME_None;

	UpdateSaveRegistry(SlotName, CurrentLevelName);

	if (World)
	{
		if (ACharacter* PlayerChar = UGameplayStatics::GetPlayerCharacter(World, 0))
		{
			SaveObj->PlayerTransform = PlayerChar->GetActorTransform();
		}
	}

	OnGameSaving.Broadcast(SaveObj);

	FAsyncSaveGameToSlotDelegate SaveDelegate;
	SaveDelegate.BindUObject(this, &UMySaveSubsystem::OnAsyncSaveComplete);
	UGameplayStatics::AsyncSaveGameToSlot(SaveObj, SlotName, 0, SaveDelegate);
}

void UMySaveSubsystem::UpdateSaveRegistry(const FString& SlotName, FName CurrentLevelName)
{
	if (!CachedRegistry)
	{
		CachedRegistry = Cast<UMySaveRegistry>(UGameplayStatics::CreateSaveGameObject(UMySaveRegistry::StaticClass()));
	}

	FSaveSlotMetaData Meta;
	Meta.SlotName = SlotName;
	Meta.SaveTime = FDateTime::Now();
	Meta.LevelName = CurrentLevelName;

	// 同步更新内存镜像
	CachedRegistry->SaveSlots.Add(SlotName, Meta);

	// 更新物理硬盘
	const FString RegistrySlot = TEXT("GlobalSaveRegistry");
	UGameplayStatics::SaveGameToSlot(CachedRegistry, RegistrySlot, 0);
}

bool UMySaveSubsystem::DeleteSaveSlot(const FString& SlotName)
{
	if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		UGameplayStatics::DeleteGameInSlot(SlotName, 0);
	}

	if (CachedRegistry)
	{
		CachedRegistry->SaveSlots.Remove(SlotName);
		UGameplayStatics::SaveGameToSlot(CachedRegistry, TEXT("GlobalSaveRegistry"), 0);
	}

	OnSaveRegistryChanged.Broadcast();
	return true;
}

void UMySaveSubsystem::OnAsyncSaveComplete(const FString& SlotName, const int32 UserIndex, bool bSuccess)
{
	OnSaveFinished.Broadcast(bSuccess);
}

bool UMySaveSubsystem::LoadGameFromSlot(const FString& SlotName)
{
	if (!UGameplayStatics::DoesSaveGameExist(SlotName, 0)) return false;

	UMySaveGame* SaveObj = Cast<UMySaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	if (!SaveObj) return false;

	UWorld* World = GetWorld();
	if (!World) return false;

	if (ACharacter* PlayerChar = UGameplayStatics::GetPlayerCharacter(World, 0))
	{
		PlayerChar->SetActorTransform(SaveObj->PlayerTransform, false, nullptr, ETeleportType::TeleportPhysics);
	}

	OnGameLoading.Broadcast(SaveObj);
	return true;
}

#pragma endregion
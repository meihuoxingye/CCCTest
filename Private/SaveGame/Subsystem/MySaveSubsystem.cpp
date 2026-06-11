// Fill out your copyright notice in the Description page of Project Settings.

#include "SaveGame/Subsystem/MySaveSubsystem.h"
#include "SaveGame/MySaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"

// ==============================================================================
// 全局存档统筹子系统 (Global Save Subsystem)
// ==============================================================================
#pragma region

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

	// 1. 同步更新元数据注册表
	UpdateSaveRegistry(SlotName, CurrentLevelName);

	// 2. 抓取与世界本身相关的基础状态
	if (World)
	{
		if (ACharacter* PlayerChar = UGameplayStatics::GetPlayerCharacter(World, 0))
		{
			SaveObj->PlayerTransform = PlayerChar->GetActorTransform();
		}
	}

	// 3. 【依赖倒置核心】：大喇叭广播！
	// 存档管家不认识任何子系统，只负责把黑盒扔出去，谁关心存档，谁就自己来往里塞数据。
	OnGameSaving.Broadcast(SaveObj);

	// 4. 抛入后台线程，执行异步直写
	FAsyncSaveGameToSlotDelegate SaveDelegate;
	SaveDelegate.BindUObject(this, &UMySaveSubsystem::OnAsyncSaveComplete);
	UGameplayStatics::AsyncSaveGameToSlot(SaveObj, SlotName, 0, SaveDelegate);
}

void UMySaveSubsystem::UpdateSaveRegistry(const FString& SlotName, FName CurrentLevelName)
{
	UMySaveRegistry* Registry = nullptr;
	const FString RegistrySlot = TEXT("GlobalSaveRegistry");

	if (UGameplayStatics::DoesSaveGameExist(RegistrySlot, 0))
	{
		Registry = Cast<UMySaveRegistry>(UGameplayStatics::LoadGameFromSlot(RegistrySlot, 0));
	}
	else
	{
		Registry = Cast<UMySaveRegistry>(UGameplayStatics::CreateSaveGameObject(UMySaveRegistry::StaticClass()));
	}

	if (Registry)
	{
		FSaveSlotMetaData Meta;
		Meta.SlotName = SlotName;
		Meta.SaveTime = FDateTime::Now();
		Meta.LevelName = CurrentLevelName;

		Registry->SaveSlots.Add(SlotName, Meta);
		UGameplayStatics::SaveGameToSlot(Registry, RegistrySlot, 0);
	}
}

void UMySaveSubsystem::OnAsyncSaveComplete(const FString& SlotName, const int32 UserIndex, bool bSuccess)
{
	if (bSuccess)
	{
		UE_LOG(LogTemp, Log, TEXT("主游戏数据异步写入完成，槽位: %s"), *SlotName);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("主游戏数据写入失败，槽位: %s"), *SlotName);
	}

	OnSaveFinished.Broadcast(bSuccess);
}

bool UMySaveSubsystem::DeleteSaveSlot(const FString& SlotName)
{
	// 1. 物理删除硬盘文件
	if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		UGameplayStatics::DeleteGameInSlot(SlotName, 0);
	}

	// 2. 抹除注册表记录
	const FString RegistrySlot = TEXT("GlobalSaveRegistry");
	if (UGameplayStatics::DoesSaveGameExist(RegistrySlot, 0))
	{
		if (UMySaveRegistry* Registry = Cast<UMySaveRegistry>(UGameplayStatics::LoadGameFromSlot(RegistrySlot, 0)))
		{
			Registry->SaveSlots.Remove(SlotName);
			UGameplayStatics::SaveGameToSlot(Registry, RegistrySlot, 0);
		}
	}

	// 3. 【核心】：敲响大喇叭！通知主面板立刻刷新滚动框！
	OnSaveRegistryChanged.Broadcast();

	return true;
}

bool UMySaveSubsystem::LoadGameFromSlot(const FString& SlotName)
{
	if (!UGameplayStatics::DoesSaveGameExist(SlotName, 0)) return false;

	UMySaveGame* SaveObj = Cast<UMySaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	if (!SaveObj) return false;

	UWorld* World = GetWorld();
	if (!World) return false;

	// 1. 恢复世界基础坐标
	if (ACharacter* PlayerChar = UGameplayStatics::GetPlayerCharacter(World, 0))
	{
		PlayerChar->SetActorTransform(SaveObj->PlayerTransform, false, nullptr, ETeleportType::TeleportPhysics);
	}

	// 2. 【依赖倒置核心】：读档大喇叭广播！
	// 把装满硬盘数据的盒子扔出去，让各个子系统自己去翻找属于自己的数据并恢复。
	OnGameLoading.Broadcast(SaveObj);

	return true;
}

TArray<FSaveSlotMetaData> UMySaveSubsystem::GetSaveSlotList()
{
	TArray<FSaveSlotMetaData> OutList;
	const FString RegistrySlot = TEXT("GlobalSaveRegistry");

	if (UGameplayStatics::DoesSaveGameExist(RegistrySlot, 0))
	{
		if (UMySaveRegistry* Registry = Cast<UMySaveRegistry>(UGameplayStatics::LoadGameFromSlot(RegistrySlot, 0)))
		{
			for (const auto& [SlotKey, MetaData] : Registry->SaveSlots)
			{
				OutList.Add(MetaData);
			}

			OutList.Sort([](const FSaveSlotMetaData& A, const FSaveSlotMetaData& B) {
				return A.SaveTime > B.SaveTime;
				});
		}
	}
	return OutList;
}

#pragma endregion
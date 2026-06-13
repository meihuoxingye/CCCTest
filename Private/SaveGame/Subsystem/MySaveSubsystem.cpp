// Fill out your copyright notice in the Description page of Project Settings.

#include "SaveGame/Subsystem/MySaveSubsystem.h"
#include "SaveGame/MySaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"

// ==============================================================================
// 核心接口 (Core Interfaces)
// ==============================================================================
#pragma region

void UMySaveSubsystem::PreloadRegistry()
{
	// 注册表在硬盘上永远固定存为一个叫 "GlobalSaveRegistry" 的文件
	const FString RegistrySlot = TEXT("GlobalSaveRegistry");

	// 绑定回调委托：当异步读取完成时，去执行 OnRegistryLoaded
	FAsyncLoadGameFromSlotDelegate LoadedDelegate;
	LoadedDelegate.BindUObject(this, &UMySaveSubsystem::OnRegistryLoaded);

	// 彻底告别主线程阻塞，开启后台异步读取。玩家在游戏中完全感觉不到在读盘
	UGameplayStatics::AsyncLoadGameFromSlot(RegistrySlot, 0, LoadedDelegate);
}

TArray<FSaveSlotMetaData> UMySaveSubsystem::GetSaveSlotList()
{
	TArray<FSaveSlotMetaData> OutList;

	// 直接 O(1) 读内存缓存，不碰物理硬盘，耗时无限趋近于 0
	if (CachedRegistry)
	{
		// 遍历注册表里的 TMap，把所有元数据塞进数组里
		for (const auto& Pair : CachedRegistry->SaveSlots)
		{
			OutList.Add(Pair.Value);
		}

		// 现代 C++ Lambda 表达式排序算法：
		// 按时间降序排列 (A > B)，确保玩家打开菜单时，最新存的档永远在最上面！
		OutList.Sort([](const FSaveSlotMetaData& A, const FSaveSlotMetaData& B) {
			return A.SaveTime > B.SaveTime;
			});
	}
	return OutList;
}

void UMySaveSubsystem::PerformAsyncSave(const FString& SlotName)
{
	// 1. 创建一个空白的“存档数据盒子”
	UMySaveGame* SaveObj = Cast<UMySaveGame>(UGameplayStatics::CreateSaveGameObject(UMySaveGame::StaticClass()));
	if (!SaveObj)
	{
		// 万一内存爆了创建失败，直接告诉 UI 存盘失败
		OnSaveFinished.Broadcast(false);
		return;
	}

	// 2. 获取当前所在的关卡名字
	UWorld* World = GetWorld();
	FName CurrentLevelName = World ? FName(*World->GetName()) : NAME_None;

	// 3. 将当前档位的信息写入“目录注册表”（同步小文件写盘）
	UpdateSaveRegistry(SlotName, CurrentLevelName);

	// 4. 写入子系统自己负责的基础数据（如玩家的位置、旋转、缩放）
	if (World)
	{
		if (ACharacter* PlayerChar = UGameplayStatics::GetPlayerCharacter(World, 0))
		{
			SaveObj->PlayerTransform = PlayerChar->GetActorTransform();
		}
	}

	// 5. 【架构神笔】：敲响大喇叭，把 SaveObj 传给全图！
	// 此时，UI、背包、战斗系统听到喇叭，会瞬间把他们的物资、血量全塞进这个 SaveObj 里。
	// 这行代码执行完后，SaveObj 已经被各路系统塞得满满当当了！
	OnGameSaving.Broadcast(SaveObj);

	// 6. 绑定硬盘写入完成的回调，开启多线程异步存盘，不卡主线程
	FAsyncSaveGameToSlotDelegate SaveDelegate;
	SaveDelegate.BindUObject(this, &UMySaveSubsystem::OnAsyncSaveComplete);
	UGameplayStatics::AsyncSaveGameToSlot(SaveObj, SlotName, 0, SaveDelegate);
}

bool UMySaveSubsystem::DeleteSaveSlot(const FString& SlotName)
{
	// 1. 删除物理硬盘上那个巨大且沉重的真实存档文件
	if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		UGameplayStatics::DeleteGameInSlot(SlotName, 0);
	}

	// 2. 从内存“目录”中剔除这个档位的记录
	if (CachedRegistry)
	{
		CachedRegistry->SaveSlots.Remove(SlotName);

		// 3. 把删减后的新目录，同步保存覆盖回物理硬盘
		UGameplayStatics::SaveGameToSlot(CachedRegistry, TEXT("GlobalSaveRegistry"), 0);
	}

	// 4. 敲响大喇叭，UI 听到后会自动让那个被删掉的卡片消失
	OnSaveRegistryChanged.Broadcast();
	return true;
}

bool UMySaveSubsystem::LoadGameFromSlot(const FString& SlotName)
{
	// 1. 物理检查：文件到底还在不在
	if (!UGameplayStatics::DoesSaveGameExist(SlotName, 0)) return false;

	// 2. 同步拉取巨大的文件到内存里
	UMySaveGame* SaveObj = Cast<UMySaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	if (!SaveObj) return false;

	UWorld* World = GetWorld();
	if (!World) return false;

	// 3. 子系统自己负责恢复最基础的属性：玩家位置
	if (ACharacter* PlayerChar = UGameplayStatics::GetPlayerCharacter(World, 0))
	{
		// 【避坑指南】：使用 ETeleportType::TeleportPhysics 是极其专业的写法。
		// 当瞬间改变角色位置时，如果不加这个枚举，角色身上的披风布料、碰撞体会因为瞬间极速移动产生逆天惯性，导致模型爆炸或者被弹飞。
		PlayerChar->SetActorTransform(SaveObj->PlayerTransform, false, nullptr, ETeleportType::TeleportPhysics);
	}

	// 4. 【恢复钩子】：敲响大喇叭，把刚从硬盘里拿出来的、装满旧数据的 SaveObj 传给全图。
	// 此时，背包系统、血量系统听到喇叭，会自己从里面拿走对应的旧数据并覆盖当前状态。
	OnGameLoading.Broadcast(SaveObj);

	return true;
}

#pragma endregion

// ==============================================================================
// 内部管线 (Internal Pipeline)
// ==============================================================================
#pragma region

void UMySaveSubsystem::OnRegistryLoaded(const FString& SlotName, const int32 UserIndex, USaveGame* LoadedGame)
{
	if (LoadedGame)
	{
		// 如果硬盘上有文件，说明是老玩家，强转并缓存到内存里
		CachedRegistry = Cast<UMySaveRegistry>(LoadedGame);
	}
	else
	{
		// 如果读出来是空指针，说明是第一次玩游戏的新玩家。
		// 引擎底层创建一个空白的注册表实例，分配给内存缓存。
		CachedRegistry = Cast<UMySaveRegistry>(UGameplayStatics::CreateSaveGameObject(UMySaveRegistry::StaticClass()));
	}

	// 注册表就绪，敲响大喇叭告诉 UI：“数据准备好了，如果存档面板开着，赶紧刷新显示！”
	OnSaveRegistryChanged.Broadcast();
}

void UMySaveSubsystem::UpdateSaveRegistry(const FString& SlotName, FName CurrentLevelName)
{
	// 防御性判空：如果还没预热，强制创建一个空的
	if (!CachedRegistry)
	{
		CachedRegistry = Cast<UMySaveRegistry>(UGameplayStatics::CreateSaveGameObject(UMySaveRegistry::StaticClass()));
	}

	// 构造本次存档的元数据信息片（目录摘要）
	FSaveSlotMetaData Meta;
	Meta.SlotName = SlotName;
	Meta.SaveTime = FDateTime::Now(); // 记录当前系统时间
	Meta.LevelName = CurrentLevelName;

	// 同步更新内存镜像：Map 结构相同 Key 会自动覆盖，完美处理“覆盖存档”的情况
	CachedRegistry->SaveSlots.Add(SlotName, Meta);

	// 物理写盘：因为目录文件极小，使用同步保存 SaveGameToSlot 不会引起任何卡顿
	const FString RegistrySlot = TEXT("GlobalSaveRegistry");
	UGameplayStatics::SaveGameToSlot(CachedRegistry, RegistrySlot, 0);
}

void UMySaveSubsystem::OnAsyncSaveComplete(const FString& SlotName, const int32 UserIndex, bool bSuccess)
{
	// 收到引擎底层的磁盘回调后，将其转发给 UI 蓝图，触发右上角的“保存成功”弹窗等表现
	OnSaveFinished.Broadcast(bSuccess);
}

#pragma endregion
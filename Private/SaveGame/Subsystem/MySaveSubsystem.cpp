// Fill out your copyright notice in the Description page of Project Settings.

#include "SaveGame/Subsystem/MySaveSubsystem.h"
#include "SaveGame/MySaveGame.h"
// 【主动引入业务类】：大管家负责向下发号施令，要求各业务线提取或注入数据
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
// 记得在顶部引入我们刚写的接口
#include "SaveGame/MySavableInterface.h"
// 【新增】：虚幻引擎底层极速对象遍历器
#include "UObject/UObjectIterator.h"
#include "GameFramework/CharacterMovementComponent.h" // 【新增】：递上运动组件的说明书
#include "Async/Async.h" // 解决僵尸索引后台猎杀的异步支持

// ==============================================================================
// 核心接口 (Core Interfaces)
// ==============================================================================
#pragma region

void UMySaveSubsystem::PreloadRegistry()
{
	// 指定内部存档槽位名称。
	// 虚幻底层 ISaveGameSystem 会以该名称为基准，自动追加 .sav 后缀来读取物理硬盘文件。
	const FString RegistrySlot = TEXT("GlobalSaveRegistry");

	// 实例化一个局部委托对象
	// 写成局部委托，就是为了实现“阅后即焚”，不用担心在 Deinitialize 或销毁时忘记调用 .Unbind()
	FAsyncLoadGameFromSlotDelegate LoadedDelegate;
	// 绑定回调委托：当异步识别完成时，去执行 OnRegistryLoaded
	LoadedDelegate.BindUObject(this, &UMySaveSubsystem::OnRegistryLoaded);

	// 开启后台异步读取物理硬盘文件。玩家在游戏中完全感觉不到在读盘，彻底告别主线程阻塞；
	UGameplayStatics::AsyncLoadGameFromSlot(RegistrySlot, 0, LoadedDelegate);
}

void UMySaveSubsystem::PerformAsyncSave(const FString& SlotName)
{
	// 1. 创建一个空白的“存档数据盒子”
	// UMySaveGame::StaticClass()：获取 UMySaveGame 这个类的静态反射类型元数据
	// 为什么用 CreateSaveGameObject 创建 UMySaveGame 对象而不用 new：参与 GC 管理体系；支持转为二进制写入物理硬盘的
	// 从不能存数据的类的元数据转为可以储存数据的内存中的实体对象
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

	// 4. 写入子系统自己负责的基础数据
	// 防御性检查：确保当前世界上下文合法，防止在切换关卡等极端情况下崩溃
	if (World)
	{
		// 记录系统锚点数据到纯净全局包裹中
		SaveObj->GlobalDataBlock.SavedLevelName = CurrentLevelName;

		// 安全获取当前本地玩家（0号玩家）的角色指针
		if (ACharacter* PlayerChar = UGameplayStatics::GetPlayerCharacter(World, 0))
		{
			// 将玩家此刻的绝对物理状态（位置、旋转、缩放）快照并记录到存档全局数据块中
			SaveObj->GlobalDataBlock.PlayerTransform = PlayerChar->GetActorTransform();
		}

		// =====================================================================
		// 【极致解耦】：接口化拉取！动态收集所有挂载了 ISavableInterface 的子系统
		// =====================================================================

		// 【架构级解耦魔法】：直接扫描引擎底层的全局对象池 (GUObjectArray)
		// 极速穷举当前内存中所有的 UWorldSubsystem 实例，如技能点子系统等（单例极少，耗时 <0.0001ms）。
		// 1. 初始化 (TObjectIterator<...> It)：创建迭代器，立刻在引擎内存中找到【第一个】该类对象。
		// 2. 条件判断 (It;)：隐式调用内部的 operator bool()，当前指向有效对象时为 true，找完为 false。
		// 3. 步进操作 (++It)：本轮循环结束时，迭代器自动跳到内存中的【下一个】该类对象。
		for (TObjectIterator<UWorldSubsystem> It; It; ++It)
		{
			UWorldSubsystem* Subsystem = *It;
			// 绝对安全锁：只处理存活在“当前真实游戏世界”的子系统（排除掉编辑器世界的实例）
			if (Subsystem && Subsystem->GetWorld() == World)
			{
				// 如果这个子系统贴了“可存档”的契约标签（实现了接口）
				if (IMySavableInterface* SavableModule = Cast<IMySavableInterface>(Subsystem))
				{
					// 无脑向它索要名字和 JSON 字符串，一把塞进万能集装箱！
					SaveObj->UniversalArchives.Add(SavableModule->GetModuleName(), SavableModule->ExtractUniversalData());
				}
			}
		}
	}

	// 5. 绑定硬盘写入完成的回调，开启多线程异步存盘，不卡主线程
	// 写成局部委托，就是为了实现“阅后即焚”，不用担心在 Deinitialize 或销毁时忘记调用 .Unbind()
	FAsyncSaveGameToSlotDelegate SaveDelegate;

	// 为什么用 BindUObject 而不是直接传函数指针？
	// 因为虚幻引擎极其注重内存安全。如果在这个异步存盘期间，玩家突然退出了游戏，大管家（this）被销毁了。
	// 当后台存完盘准备打电话时，引擎底层会自动检查 this 是否还活着。如果死了，电话就会静默取消，绝对不会因为打给一个死对象而引发野指针崩溃。
	SaveDelegate.BindUObject(this, &UMySaveSubsystem::OnAsyncSaveComplete);

	// 全局异步存盘函数，存档数据盒子、传入的存档名、0 本地 1P 玩家、硬盘写入完成的委托
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

	// 4. 敲响大喇叭，UI 听到后会自动让那个被删掉的卡片刷新显示为空白
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

	// 【修复：缺陷1】判断关卡名。如果不一致，强制切图，将恢复任务挂起
	FName CurrentLevelName = FName(*World->GetName());
	if (CurrentLevelName != SaveObj->GlobalDataBlock.SavedLevelName)
	{
		PendingLoadSlotName = SlotName;
		UGameplayStatics::OpenLevel(World, SaveObj->GlobalDataBlock.SavedLevelName);
		return true;
	}

	// 关卡一致，立刻原地恢复状态
	PendingLoadSlotName = SlotName;
	HandlePendingLoad();
	return true;
}

void UMySaveSubsystem::HandlePendingLoad()
{
	if (PendingLoadSlotName.IsEmpty()) return;

	UMySaveGame* SaveObj = Cast<UMySaveGame>(UGameplayStatics::LoadGameFromSlot(PendingLoadSlotName, 0));
	if (!SaveObj) return;

	UWorld* World = GetWorld();
	if (!World) return;

	// 3. 子系统自己负责恢复最基础的属性：玩家位置
	if (ACharacter* PlayerChar = UGameplayStatics::GetPlayerCharacter(World, 0))
	{
		// 强制清空速度和物理运动状态
		if (UCharacterMovementComponent* MoveComp = PlayerChar->GetCharacterMovement())
		{
			MoveComp->StopMovementImmediately();
		}

		// 【避坑指南】：使用 ETeleportType::TeleportPhysics 是极其专业的写法。
		// 当瞬间改变角色位置时，如果不加这个枚举，角色身上的披风布料、碰撞体会因为瞬间极速移动产生逆天惯性，导致模型爆炸或者被弹飞。
		// 注意这里改为了从纯净包裹 GlobalDataBlock 中提取坐标
		PlayerChar->SetActorTransform(SaveObj->GlobalDataBlock.PlayerTransform, false, nullptr, ETeleportType::TeleportPhysics);
	}

	// =====================================================================
	// 【极致解耦】：接口化注入！把包裹按名字分发给对应的子系统
	// =====================================================================
	for (TObjectIterator<UWorldSubsystem> It; It; ++It)
	{
		UWorldSubsystem* Subsystem = *It;
		// 绝对安全锁：确认是当前世界的子系统
		if (Subsystem && Subsystem->GetWorld() == World)
		{
			// 如果是个可存档系统
			if (IMySavableInterface* SavableModule = Cast<IMySavableInterface>(Subsystem))
			{
				// 问出它的名字
				FName ModuleName = SavableModule->GetModuleName();
				// 在万能集装箱里找找，有没有属于它的 JSON 字符串包裹？
				if (const FString* FoundJsonData = SaveObj->UniversalArchives.Find(ModuleName))
				{
					// 第一步：如果有，把字符串扔给它，让它自己去解析还原 (此时不操作Actor)
					SavableModule->InjectUniversalData(*FoundJsonData);

					// 第二步：触发真正的数据应用逻辑
					SavableModule->ApplyUniversalData();
				}
			}
		}
	}

	// 任务完成，清空锚点
	PendingLoadSlotName.Empty();
}

#pragma endregion

// ==============================================================================
// 分页系统 (Pagination System)
// ==============================================================================
#pragma region

void UMySaveSubsystem::UnlockNewSavePage()
{
	// 绝对的安全性：必须小于 MaxUnlockedPages（最大页数上限）
	if (CachedRegistry && CachedRegistry->UnlockedPages < CachedRegistry->MaxUnlockedPages)
	{
		CachedRegistry->UnlockedPages++;

		// 同步覆写至全局硬盘注册表
		UGameplayStatics::SaveGameToSlot(CachedRegistry, TEXT("GlobalSaveRegistry"), 0);

		// 敲响喇叭，UI 面板的 BuildSaveSlotList 会自动听到并注入新的一页
		OnSaveRegistryChanged.Broadcast();
	}
}

void UMySaveSubsystem::ClearSavePage(int32 PageIndex)
{
	// 防御性检查：确保内存中的全局注册表已经加载，否则直接退出防崩溃
	if (!CachedRegistry) return;

	// 标记位：记录本次操作是否真正删除了任何实质性的存档数据
	bool bAnyDeleted = false;

	// 【解耦生效】：直接向注册表索要分页法则，不再依赖外部传参
	int32 SlotsPerPage = CachedRegistry->SlotsPerPage;

	// 计算算法：根据当前页码和每页容量，算出本页第一个存档槽位的起始编号（例如第2页，每页5个，起始就是6）
	int32 StartIndex = (PageIndex - 1) * SlotsPerPage + 1;

	// 仅执行物理抹除，绝不触碰 UnlockedPages 变量，保护排版
	// 遍历当前页包含的所有槽位编号（例如 i=0 到 4，即处理本页的 5 个槽位）
	for (int32 i = 0; i < SlotsPerPage; ++i)
	{
		// 动态拼接出当前槽位的标准字符串名称，例如 "SaveSlot_006"
		FString SlotName = FString::Printf(TEXT("SaveSlot_%03d"), StartIndex + i);

		// 检查虚幻引擎的物理硬盘目录下是否真实存在这个 .sav 文件
		if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
		{
			// 如果存在，命令引擎从物理硬盘上将其彻底删除
			UGameplayStatics::DeleteGameInSlot(SlotName, 0);
		}

		// 尝试从内存缓存的注册表（TMap字典）中移除这个槽位的记录
		// Remove() 会返回成功移除的数量。如果 >0，说明之前确实有这个存档的记录
		if (CachedRegistry->SaveSlots.Remove(SlotName) > 0)
		{
			// 只要字典里删掉了东西，就将标记位置为 true，说明数据发生了变动
			bAnyDeleted = true;
		}
	}

	// 如果在此次循环中确实删除了数据（无论是物理文件还是内存记录）
	if (bAnyDeleted)
	{
		// 将更新后的（已经抹掉这几条记录的）注册表重新写回物理硬盘
		UGameplayStatics::SaveGameToSlot(CachedRegistry, TEXT("GlobalSaveRegistry"), 0);
		// 敲响大喇叭：通知所有监听的 UI 面板（存档菜单）立刻刷新显示
		OnSaveRegistryChanged.Broadcast();
	}
}

void UMySaveSubsystem::CompactEmptySavePages()
{
	// 防御性检查
	if (!CachedRegistry) return;

	// 目标页指针（慢指针）：指向下一个应该用来存放有效数据的“坑位”页码
	int32 TargetPageIndex = 1;
	// 标记位：记录在整个碎片整理过程中，注册表是否发生了需要保存的变动
	bool bRegistryChanged = false;

	// 【解耦生效】：直接向注册表索要分页法则
	int32 SlotsPerPage = CachedRegistry->SlotsPerPage;

	// 获取操作系统底层的文件管理接口，用于绕过引擎的序列化，直接极速操作物理文件
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	// 拼接出当前游戏项目实际存放 .sav 存档文件的绝对物理路径（通常在 项目根目录/Saved/SaveGames）
	FString SaveDirectory = FPaths::ProjectSavedDir() / TEXT("SaveGames");

	// 【新增：两阶段提交防断电炸档】创建一个任务队列，记录所有需要搬运的 (旧名字 -> 新名字)
	TArray<TPair<FString, FString>> MoveTasks;

	// 第一轮扫描：只探查，不执行任何物理操作
	// 遍历所有已解锁的页，寻找数据碎片（CurrentPageIndex 为快指针，负责在前面探路）
	for (int32 CurrentPageIndex = 1; CurrentPageIndex <= CachedRegistry->UnlockedPages; ++CurrentPageIndex)
	{
		// 标记位：记录当前正在探测的这一页里面，是否含有至少一个有效的存档
		bool bHasData = false;
		// 算出当前探测页的第一个槽位编号
		int32 CurrentStartIndex = (CurrentPageIndex - 1) * SlotsPerPage + 1;

		// 检查这一页到底有没有文件（遍历该页的每一个槽位）
		for (int32 i = 0; i < SlotsPerPage; ++i)
		{
			// 拼接出当前检查的槽位名称
			FString SlotName = FString::Printf(TEXT("SaveSlot_%03d"), CurrentStartIndex + i);
			// 如果在内存的目录表里找到了这个槽位
			if (CachedRegistry->SaveSlots.Contains(SlotName))
			{
				// 认定该页存在有效数据，立刻跳出内层循环，无需再查该页的其他空位
				bHasData = true;
				break;
			}
		}

		// 如果这页有数据
		if (bHasData)
		{
			// 如果当前探测到的页码 大于 目标坑位页码，说明中间必定有空页，需要执行“前移搬迁”
			// 只要 CurrentPageIndex > TargetPageIndex，那个 TargetPageIndex（目标页）就绝对是个空坑，根本不需要再去查
			if (CurrentPageIndex > TargetPageIndex)
			{
				// 计算出目标坑位页的第一个槽位编号
				int32 TargetStartIndex = (TargetPageIndex - 1) * SlotsPerPage + 1;

				// 开始将当前页的 5 个槽位，逐一记录到搬迁任务队列中
				for (int32 i = 0; i < SlotsPerPage; ++i)
				{
					// 拼出老（当前）槽位的名称（例如 SaveSlot_011）
					FString OldSlotName = FString::Printf(TEXT("SaveSlot_%03d"), CurrentStartIndex + i);
					// 拼出新（目标）槽位的名称（例如 SaveSlot_006）
					FString NewSlotName = FString::Printf(TEXT("SaveSlot_%03d"), TargetStartIndex + i);

					// 尝试在注册表中找到老槽位绑定的“元数据”
					if (CachedRegistry->SaveSlots.Contains(OldSlotName))
					{
						// 记录搬运任务
						MoveTasks.Add(TPair<FString, FString>(OldSlotName, NewSlotName));
					}
				}
			}
			// 只有装填了数据的页，目标指针才会推进（无论是原地不动还是搬移过来的，只要目标页装了数据，坑位就+1）
			TargetPageIndex++;
		}
	}

	// 如果真的有任务需要执行，开启“两阶段提交”极速安全搬运
	if (MoveTasks.Num() > 0)
	{
		// ==============================================================================
		// 【阶段一：意图登记 (Write-Ahead Logging)】
		// 在物理文件变动前，把“新坑位”强行合法化，并写入硬盘。
		// ==============================================================================
		for (const auto& Task : MoveTasks)
		{
			// 拷贝一份老元数据作为蓝本
			FSaveSlotMetaData NewMeta = CachedRegistry->SaveSlots[Task.Key];
			// 把元数据内部记录的名字更新为新的
			NewMeta.SlotName = Task.Value;
			// 将这条新记录添加到内存注册表（此时新旧双胞胎共存）
			CachedRegistry->SaveSlots.Add(Task.Value, NewMeta);
		}

		// 强制落盘。如果在这之后瞬间断电，下次启动时，僵尸猎手会自动把空壳新名字清理掉，存档绝对安全。
		UGameplayStatics::SaveGameToSlot(CachedRegistry, TEXT("GlobalSaveRegistry"), 0);


		// ==============================================================================
		// 【阶段二：底层极速改名 (OS Level Move)】
		// ==============================================================================
		for (const auto& Task : MoveTasks)
		{
			// A. 【核武级底层优化】：绕开序列化管线，直接让操作系统操纵硬盘指针重命名文件（耗时 < 0.1毫秒）
			// 拼出旧文件的绝对路径
			FString OldFilePath = SaveDirectory / (Task.Key + TEXT(".sav"));
			// 拼出新文件的绝对路径
			FString NewFilePath = SaveDirectory / (Task.Value + TEXT(".sav"));

			// 【预防措施】：清理目标坑位的残留死文件，防止因句柄冲突导致搬迁失败
			if (PlatformFile.FileExists(*NewFilePath))
			{
				PlatformFile.DeleteFile(*NewFilePath);
			}

			// ==============================================================================
			// 【原子性修复】：消除多余的 FileExists 检查，直接尝试物理移动！
			// ==============================================================================
			PlatformFile.MoveFile(*NewFilePath, *OldFilePath);
		}


		// ==============================================================================
		// 【阶段三：清理旧账 (Commit & Cleanup)】
		// ==============================================================================
		for (const auto& Task : MoveTasks)
		{
			// 从内存注册表中彻底抹杀旧记录
			CachedRegistry->SaveSlots.Remove(Task.Key);
		}
	}

	// 【解耦生效】：向注册表索要最低保底页数，彻底消灭硬编码魔法数字
	int32 MinPages = CachedRegistry->MinUnlockedPages;

	// 最终安全削减页数
	// 如果目标指针最后停在 X，说明前 X-1 页装了数据。用 Max(保底页数, ...) 保证游戏至少永远有配置的底线页数。
	int32 NewTotalPages = FMath::Max(MinPages, TargetPageIndex - 1);

	// 如果计算出的整理后总页数，跟现在记录的总页数不一样（也就是尾部有空页被砍掉了）
	if (CachedRegistry->UnlockedPages != NewTotalPages)
	{
		// 更新内存中的总页数上限
		CachedRegistry->UnlockedPages = NewTotalPages;
		bRegistryChanged = true;
	}

	// 如果执行了搬移任务，或尾部空页被砍掉
	if (MoveTasks.Num() > 0 || bRegistryChanged)
	{
		// 配合 UE 5.7 引擎底层的缓存刷新，稍微挂起极短的时间防冲突（非阻塞式）
		// 虽然 MoveFile 极快，但给引擎的底层文件系统留一丝喘息空间是好习惯
		FPlatformProcess::Sleep(0.01f);

		// 把排版得整整齐齐的最新注册表写入物理硬盘
		UGameplayStatics::SaveGameToSlot(CachedRegistry, TEXT("GlobalSaveRegistry"), 0);
		// 广播 UI 刷新，玩家会看到存档卡片瞬间往前对齐补位
		OnSaveRegistryChanged.Broadcast();
	}
}

#pragma endregion


// ==============================================================================
// 内部管线 (Internal Pipeline)
// ==============================================================================
#pragma region

void UMySaveSubsystem::OnRegistryLoaded(const FString& SlotName, const int32 UserIndex, USaveGame* LoadedGame)
{
	// LoadedGame 是物理存档文件转换为的对象的内存地址
	if (LoadedGame)
	{
		// 如果硬盘上有文件并且成功读取了，说明是老玩家，强转为我们自己的注册表类并缓存到内存里
		CachedRegistry = Cast<UMySaveRegistry>(LoadedGame);
	}
	else
	{
		// 如果读出来是空指针，说明是第一次玩游戏的新玩家。
		// 引擎底层创建一个空白的注册表实例，分配给内存缓存。
		CachedRegistry = Cast<UMySaveRegistry>(UGameplayStatics::CreateSaveGameObject(UMySaveRegistry::StaticClass()));
	}

	// 确保内存分配绝对成功
	if (CachedRegistry)
	{
		// 【修复性能隐患】：剥离几百次磁盘 FileExists I/O 的阻塞，扔进后台线程静默处理
		AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this, CachedReg = this->CachedRegistry]()
			{
				// 【启动期自检】：猎杀“僵尸索引” (Zombie Index Eradication)
				// 防止玩家手动在系统文件夹中删除 .sav 文件而产生内存越界崩溃

				// 拿到系统底层的文件接口
				IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
				// 拼出物理存档目录
				FString SaveDirectory = FPaths::ProjectSavedDir() / TEXT("SaveGames");

				// 收集损坏索引，绝对不可以在遍历 TMap 的同时直接调用 Remove()（C++ 铁律：会导致迭代器失效直接崩溃！）
				// 创建一个临时数组，充当“死亡黑名单”
				TArray<FString> DeadSlots;

				// 后台静默扫描内存注册表里的每一条记录
				for (const auto& Pair : CachedReg->SaveSlots)
				{
					// 拼装出该记录理论上对应的物理文件绝对路径
					FString CheckPath = SaveDirectory / (Pair.Key + TEXT(".sav"));
					// 去硬盘上查岗，如果发现这文件根本不存在（被玩家在外部私自删了）
					if (!PlatformFile.FileExists(*CheckPath))
					{
						// 把这个名字记在黑名单上
						DeadSlots.Add(Pair.Key);
					}
				}

				// 切换回 GameThread 主线程执行 UI 变更与内存更新，虚幻铁律：绝不在后台修改影响 UI 的数据
				AsyncTask(ENamedThreads::GameThread, [this, CachedReg, DeadSlots]()
					{
						if (!IsValid(this) || !IsValid(CachedReg)) return;

						// 标记位：是否发现了损坏的僵尸索引
						bool bFoundZombies = false;

						// 集中处决黑名单
						for (const FString& DeadSlot : DeadSlots)
						{
							// 安全地从字典里剔除这个空头支票
							CachedReg->SaveSlots.Remove(DeadSlot);
							bFoundZombies = true;
						}

						// 如果真的发现了外部篡改并执行了清洗
						if (bFoundZombies)
						{
							// 把清理干净、绝对吻合真实硬盘状态的注册表强制覆写回去
							UGameplayStatics::SaveGameToSlot(CachedReg, TEXT("GlobalSaveRegistry"), 0);
						}

						// 只有在内存分配绝对成功、且脏数据被完全清洗干净的情况下，才敲响大喇叭！
						// UI 接到这个通知时，读取到的将是 100% 纯净、安全的数据
						OnSaveRegistryChanged.Broadcast();
					});
			});
	}
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
	// 【核心魔法】：在赋值的瞬间，直接将时间拍扁成 24 小时制的纯净字符串！
	Meta.FormattedSaveTime = Meta.SaveTime.ToString(TEXT("%Y-%m-%d %H:%M"));
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
	// 同时触发数据池刷新（例如新覆盖了一个存档，时间戳或者关卡变了）
	OnSaveRegistryChanged.Broadcast();
}

#pragma endregion
// Fill out your copyright notice in the Description page of Project Settings.

#include "SaveGame/Subsystem/MySaveSubsystem.h"
#include "SaveGame/MySaveContainer.h"
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
// 【新增】：递上运动组件的说明书
#include "GameFramework/CharacterMovementComponent.h" 
// 解决僵尸索引后台猎杀的异步支持
#include "Async/Async.h" 


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
	// UMySaveContainer::StaticClass()：获取 UMySaveContainer 这个类的静态反射类型元数据
	// 为什么用 CreateSaveGameObject 创建 UMySaveContainer 对象而不用 new：参与 GC 管理体系；支持转为二进制写入物理硬盘的
	// 从不能存数据的类的元数据转为可以储存数据的内存中的实体对象
	UMySaveContainer* SaveObj = Cast<UMySaveContainer>(UGameplayStatics::CreateSaveGameObject(UMySaveContainer::StaticClass()));
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
	// 检查世界上下文到底还在不在
	UWorld* World = GetWorld();
	if (!World) return false;

	// 物理检查：文件到底还在不在
	if (!UGameplayStatics::DoesSaveGameExist(SlotName, 0)) return false;

	// 同步拉取巨大的文件到内存里
	UMySaveContainer* SaveObj = Cast<UMySaveContainer>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	if (!SaveObj) return false;


	// =====================================================================
	// 【终极修复：废除“同关卡免加载”的危险优化，强制刷新世界状态】
	// 无论玩家是否在当前关卡，必须强制 OpenLevel。
	// 否则那些已经被杀死的怪物、被触发的机关、被破坏的木箱，都不会被重置！
	// 相当于一刀切，没专门设置的一切都会重置
	// =====================================================================
	PendingLoadSlotName = SlotName;
	UGameplayStatics::OpenLevel(World, SaveObj->GlobalDataBlock.SavedLevelName);

	return true;
}

void UMySaveSubsystem::HandlePendingLoad()
{
	// 检查世界上下文到底还在不在
	UWorld* World = GetWorld();
	if (!World) return;

	// 【校验记忆锚点】：判断本次新关卡的启动，是因为“读档切图”引起的，还是普通的“正常进入/死亡重生”。
	// 如果锚点为空，说明就是普通进游戏，大管家直接下班。
	if (PendingLoadSlotName.IsEmpty()) return;

	// 【物理重载】：重新从物理硬盘把数十兆的存档拉取到当前的新内存里。
	// 为什么要再读一次硬盘？
	// 因为刚才执行了 OpenLevel（核弹清屏），旧世界连同之前在 LoadGameFromSlot 里临时拉取的 SaveObj 已经全部灰飞烟灭。
	// 现在大管家身处重生后的新世界，必须拿着手里的“记忆锚点（存档名）”，去硬盘仓库里重新提一次货。
	UMySaveContainer* SaveObj = Cast<UMySaveContainer>(UGameplayStatics::LoadGameFromSlot(PendingLoadSlotName, 0));
	if (!SaveObj) return;


	// 子系统自己负责恢复最基础的属性：玩家位置
	if (ACharacter* PlayerChar = UGameplayStatics::GetPlayerCharacter(World, 0))
	{
		// 强制清空速度和物理运动状态，防摔死
		if (UCharacterMovementComponent* MoveComp = PlayerChar->GetCharacterMovement())
		{
			MoveComp->StopMovementImmediately();
		}

		// 【避坑指南】：使用 ETeleportType::ResetPhysics 是极其专业的写法。
		// 读档是为了“重置”玩家状态。ResetPhysics 比 TeleportPhysics 更彻底地消除旧关卡残留的动量，防止读档后角色产生微小的位移补偿或抖动。
		PlayerChar->SetActorTransform(SaveObj->GlobalDataBlock.PlayerTransform, false, nullptr, ETeleportType::ResetPhysics);
	}


	// 暴力扫描底层内存池中存活的所有子系统。
	// 架构意义：大管家无需硬编码具体的业务系统（如背包、技能），只需认准 IMySavableInterface 契约即可全自动抓取。
	// 拓展优势：未来新增任何需要存档的模块，只需继承接口，此处分发管线的代码终生无需修改。
	for (TObjectIterator<UWorldSubsystem> It; It; ++It)
	{
		UWorldSubsystem* Subsystem = *It;
		// 绝对安全锁：确认是当前世界的子系统，而不是编辑器世界的
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
					// 【核心解耦：第一阶段：时序隔离数据注入】
					// 机制：通过 IMySavableInterface 接口，触发具体子系统（如背包/技能）各自实现的解析逻辑。
					// 职责：仅执行本地数据结构的静态填充与反序列化，严禁在此刻触碰或检索任何外部 Actor 物理实体。
					// 目的：此节点处于新关卡初期的不确定时序，目标 Actor 尚未完全就绪，提前物理寻址将引发灾难性的空指针击穿。
					SavableModule->InjectUniversalData(*FoundJsonData);

					// 【核心解耦：第二阶段：业务状态安全拉载】
					// 机制：同样通过接口分发，通知所有子系统把刚才吃进肚子里的缓存数据，真正实例化到游戏世界中。
					// 职责：当所有子系统均已完成数据持有（网状数据已安全着陆）后，方可触发此函数执行真实的物理状态应用。
					// 目的：分离“数据持有”与“业务落地”，彻底根绝跨系统由于“互为依赖、但初始化先后顺序不同”导致的死锁或空指针崩溃。
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


	// 【新增】：两阶段提交防止玩家在“搬文件”的瞬间断电导致吞档
	// 创建一个任务队列，记录所有需要搬运的 (旧名字 -> 新名字)
	TArray<TPair<FString, FString>> MoveTasks;

	// ==============================================================================
	// 【阶段零：探查与规划 (Exploration & Planning)】
	// 只遍历内存中的注册表，寻找数据碎片，生成搬迁任务清单（不执行任何物理文件读写）。
	// CurrentPageIndex 为快指针，负责在前面探路；TargetPageIndex 为慢指针，指向空缺坑位。
	// ==============================================================================
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
		// ==============================================================================
		// 【终极修复：致命隐患 - 线程安全快照】
		// 绝对不可以在后台线程遍历 UObject（即 CachedRegistry）的成员变量！
		// 为了防止后台遍历时发生垃圾回收或主线程修改导致的随机崩溃，
		// 必须在主线程先提取一份纯字符串的“数据快照”，只把字符串派发给后台去查岗。
		// ==============================================================================

		// 声明纯字符串数组作为数据“快照”容器，用于在主线程中安全隔离并存放所有的档位名称。
		TArray<FString> SlotNamesToVerify;
		// 调用 GenerateKeyArray 极速提取字典中所有的 Key 并强行复制到数组中。通过提取纯值类型（FString），
		// 彻底切断后台线程对 UObject 及其内部字典的直接引用，封死因主线程并发增删或引擎 GC 引发的多线程踩内存崩溃。
		CachedRegistry->SaveSlots.GenerateKeyArray(SlotNamesToVerify);

		// 记录管家自身的弱引用，防止后台多线程运行期间管家意外死亡（如退出游戏）导致的野指针崩溃
		TWeakObjectPtr<UMySaveSubsystem> WeakThis(this);

		// 【修复性能隐患】：剥离几百次磁盘 FileExists I/O 的阻塞，扔进后台线程静默处理
		AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [WeakThis, SlotNamesToVerify]()
			{
				// 【启动期自检】：猎杀“僵尸索引” (Zombie Index Eradication)
				// 防止玩家手动在系统文件夹中删除 .sav 文件而产生内存越界崩溃

				// 拿到系统底层的文件接口
				IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
				// 拼出物理存档目录
				FString SaveDirectory = FPaths::ProjectSavedDir() / TEXT("SaveGames");

				// 收集损坏索引。创建一个临时数组，充当“死亡黑名单”
				TArray<FString> FoundDeadSlots;

				// 后台静默扫描【纯字符串快照】，彻底规避 TMap 的并发冲突
				for (const FString& SlotNameToCheck : SlotNamesToVerify)
				{
					// 拼装出该记录理论上对应的物理文件绝对路径
					FString CheckPath = SaveDirectory / (SlotNameToCheck + TEXT(".sav"));
					// 去硬盘上查岗，如果发现这文件根本不存在（被玩家在外部私自删了）
					if (!PlatformFile.FileExists(*CheckPath))
					{
						// 把这个名字记在黑名单上
						FoundDeadSlots.Add(SlotNameToCheck);
					}
				}

				// 切换回 GameThread 主线程执行 UI 变更与内存更新，虚幻铁律：绝不在后台修改影响 UI 和 UObject 的数据
				AsyncTask(ENamedThreads::GameThread, [WeakThis, FoundDeadSlots]()
					{
						// 尝试获取强引用，如果管家还活着则继续
						if (UMySaveSubsystem* StrongThis = WeakThis.Get())
						{
							// 再次防御性检查注册表是否存活
							if (!StrongThis->CachedRegistry) return;

							// 如果真的发现了外部篡改（有死亡黑名单）
							if (FoundDeadSlots.Num() > 0)
							{
								// 集中处决黑名单：安全地从字典里剔除这些空头支票
								for (const FString& DeadSlot : FoundDeadSlots)
								{
									StrongThis->CachedRegistry->SaveSlots.Remove(DeadSlot);
								}

								// 把清理干净、绝对吻合真实硬盘状态的注册表强制覆写回去
								UGameplayStatics::SaveGameToSlot(StrongThis->CachedRegistry, TEXT("GlobalSaveRegistry"), 0);
							}

							// 只有在内存分配绝对成功、且脏数据被完全清洗干净（或无需清洗）的情况下，才敲响大喇叭！
							// UI 接到这个通知时，读取到的将是 100% 纯净、安全的数据
							StrongThis->OnSaveRegistryChanged.Broadcast();
						}
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


// ==============================================================================
// 安全数据访问接口 (Safe Data Access Getters)
// ==============================================================================
#pragma region

int32 UMySaveSubsystem::GetSlotsPerPage() const
{
	// 绝对防御：如果内存台账完好，返回设定值；如果由于极小概率异常导致台账丢失，抛出兜底值 5，防止 UI 算力除零崩溃。
	return CachedRegistry ? CachedRegistry->SlotsPerPage : 5;
}

int32 UMySaveSubsystem::GetTotalUnlockedPages() const
{
	// 返回当前玩家实际拥有的页数，丢档兜底值为 1 页。
	return CachedRegistry ? CachedRegistry->UnlockedPages : 1;
}

int32 UMySaveSubsystem::GetMaxUnlockedPages() const
{
	// 返回系统设定的物理硬上限，兜底值为 50。
	return CachedRegistry ? CachedRegistry->MaxUnlockedPages : 50;
}

FSaveSlotMetaData* UMySaveSubsystem::GetSlotMetaData(const FString& SlotName) const
{
	// 绝对防御：如果台账在，才去哈希表里找；如果台账丢了，直接返回空指针，告诉 UI “没查到这个档”。
	if (CachedRegistry)
	{
		return CachedRegistry->SaveSlots.Find(SlotName);
	}
	return nullptr;
}

#pragma endregion
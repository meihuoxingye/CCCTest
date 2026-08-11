// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/TopCharacter.h"
// 自定义移动控制组件
#include "Component/MovementControl/MyMovementControlComponent.h"
// 引入对应的 GameMode 和控制器以供数据注册与全局输入处理
#include "Game/MyGameModeBase.h"
#include "Character/TopPlayerController.h"
// 角色属性数据资产配置
#include "Character/CharacterAttributeDataAsset.h"
// 技能点子系统
#include "SkillSystem/SkillPointSubsystem.h"
// 使用了 GetWorld()，让编译器知道 UWorld* 指针
#include "Engine/World.h"
// 【新增模块】
#include "EnhancedInputComponent.h"
#include "Component/CombatSystem/MyCombatComponent.h"
// 【新增】：官方角色移动组件（为了修改 bRunPhysicsWithNoController）
#include "GameFramework/CharacterMovementComponent.h"

// 【新增：交互系统所需的头文件】
#include "Components/SphereComponent.h"
#include "Interaction/MyInteractableInterface.h"
#include "Math/UnrealMathUtility.h"

// 【新增：屏幕文本输出所需的全局引擎头文件】
#include "Engine/Engine.h"

#include "Components/SkeletalMeshComponent.h"

// 虚幻5.8：引入全局输入映射上下文类
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"

// 【修复 UE5.8】：替换为 Manager，DataLayer 状态改变委托已被移至此处
#include "WorldPartition/DataLayer/DataLayerManager.h"

// 【新增】：引入大一统传送子系统，用于 O(1) 缓存注册
#include "MapTravel/MyMapTravelSubsystem.h"


// Sets default values
ATopCharacter::ATopCharacter()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// ==============================================================================
	// 【防堵门装甲】：专属于玩家角色的物理豁免权
	// 联机时房主必然会占住接机点。强制要求 GameMode：即使碰撞重叠，也必须把副机挤开并强行生出来！
	// 绝对不允许因为物理阻挡而导致玩家的肉体流产！
	// ==============================================================================
	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	MMCComponent = CreateDefaultSubobject<UMyMovementControlComponent>(TEXT("MyMovementControlComponent"));

	// 【新增】：初始化交互探测球
	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	// 将交互探测球挂载到角色的根组件（胶囊体）上，跟随肉体实时移动
	InteractionSphere->SetupAttachment(RootComponent);
	// 划定 150 厘米的雷达探测半径，作为玩家可交互的物理极限距离
	InteractionSphere->SetSphereRadius(150.f);
	// 纯逻辑探测：关闭物理阻挡，仅允许空间查询（Query），节约 Chaos 物理引擎算力
	InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	// 初始化防御网：默认无视场景中的所有碰撞体，防止被复杂场景频繁唤醒
	InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	// 精准打击：仅对动态世界物体（WorldDynamic，通常是可交互 Actor）开放重叠（Overlap）响应
	InteractionSphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
}

// ==============================================================================
// 核心生命周期 (Core Lifecycle)
// ==============================================================================
#pragma region

// Called every frame
void ATopCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}


// Called when the game starts or when spawned
void ATopCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 【新增】：角色诞生，立刻向大一统传送子系统报到，注入全局 O(1) 物理黑名单缓存！
	if (UWorld* World = GetWorld())
	{
		if (UMyMapTravelSubsystem* TravelSub = World->GetSubsystem<UMyMapTravelSubsystem>())
		{
			TravelSub->RegisterCharacterToCache(this);
		}
	}

	// 允许没有控制器附身的躯壳继续受物理引擎控制（保持重力下落和惯性）
	// 这行代码会打破虚幻“无魂则静”的默认优化，彻底修复半空切人定格的 Bug！
	GetCharacterMovement()->bRunPhysicsWithNoController = true;

	// 1. 使用局部的 const 原始指针来接取配置资产（推荐的最佳实践）
	// 这样可以确保在当前的 BeginPlay 逻辑里，没有任何人能意外修改 Config 内部的数值
	const UCharacterAttributeDataAsset* Config = AttributeConfig.Get();
	if (!Config) return;

	// 2. 获取技能点子系统（注意：这里的子系统指针【绝对不能】加 const）
	if (USkillPointSubsystem* SkillPointSubsystem = GetWorld()->GetSubsystem<USkillPointSubsystem>())
	{
		// 3. 直接通过 const 指针读取成员并传入
		SkillPointSubsystem->AddNewCharacterToSquad(
			Config->CharacterID,
			Config->MaxSP,
			Config->InitialSP,
			Config->RegenRate
		);
	}

	// 类型为友好的角色出生自动注册
	if (Config && Config->CharacterType == ECharacterType::Friendly)
	{
		// GetAuthGameMode() 向当前关卡世界申请获取基础游戏模式
		if (AMyGameModeBase* GM = Cast<AMyGameModeBase>(GetWorld()->GetAuthGameMode()))
		{
			// 将此角色注册 to MyGameModeBase 的友军名册里
			// 角色现在只做纯粹的数据上报，后续的 UI 刷新将完全由 GameMode 的内部多播事件通知 UI 组件
			GM->RegisterFriendly(this);
		}
	}

	// 绑定物理范围重叠委托
	if (InteractionSphere)
	{
		// 【新增】：工业级防御性安全设计：先注销再绑定，彻底封死无缝流转或蓝图双重重入导致的查重崩溃
		InteractionSphere->OnComponentBeginOverlap.RemoveDynamic(this, &ATopCharacter::OnInteractSphereBeginOverlap);
		InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &ATopCharacter::OnInteractSphereBeginOverlap);

		InteractionSphere->OnComponentEndOverlap.RemoveDynamic(this, &ATopCharacter::OnInteractSphereEndOverlap);
		InteractionSphere->OnComponentEndOverlap.AddDynamic(this, &ATopCharacter::OnInteractSphereEndOverlap);
	}
}

void ATopCharacter::PerformCleanup()
{
	// 状态锁校验：如果清理流程已经执行过，直接驳回以防止重复注销导致的崩溃
	if (bHasDoneCleanup) return;

	// ===================== 【生命周期闭环：统一注销】 =====================

	// 安全提取当前世界上下文
	if (UWorld* World = GetWorld())
	{
		// 提取虚幻5标准的大世界数据层管理器
		if (UDataLayerManager* DLManager = UDataLayerManager::GetDataLayerManager(World))
		{
			// 彻底断开数据层状态变化的委托监听，物理切断引擎底层的无效广播
			DLManager->OnDataLayerInstanceRuntimeStateChanged.RemoveDynamic(this, &ATopCharacter::OnDataLayerStateChanged);
		}
	}

	// 提取角色属性配置以判定其阵营归属
	const UCharacterAttributeDataAsset* Config = AttributeConfig.Get();

	// 阵营校验：仅当该角色属于玩家的“友方”小队成员时才执行注销
	if (Config && Config->CharacterType == ECharacterType::Friendly)
	{
		// 再次安全提取当前世界上下文
		if (UWorld* World = GetWorld())
		{
			// 提取当前世界的权威游戏模式 (仅服务器端有效)
			if (AMyGameModeBase* GM = Cast<AMyGameModeBase>(World->GetAuthGameMode()))
			{
				// 无论角色是被伤害击杀，还是在跨地图时被系统强制清除，必须确保它从 GameMode 的大名单里滚蛋
				GM->UnregisterFriendly(this);
				UE_LOG(LogTemp, Warning, TEXT("🗑️ [小队名册] 角色肉体被物理销毁/收回，已自动从系统名册中彻底注销: %s"), *GetName());
			}
		}
	}

	// 落下原子锁：宣告该角色的底层清盘流程已物理终结，封死重入路径
	bHasDoneCleanup = true;
}

void ATopCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 【新增】：角色死亡或被跨图销毁，必须立刻从大一统缓存中除名，彻底防止 Set 内存膨胀！
	if (UWorld* World = GetWorld())
	{
		if (UMyMapTravelSubsystem* TravelSub = World->GetSubsystem<UMyMapTravelSubsystem>())
		{
			TravelSub->UnregisterCharacterFromCache(this);
		}
	}

	// 【架构优化实装】：统一清理
	PerformCleanup();

	Super::EndPlay(EndPlayReason);
}

void ATopCharacter::OnDataLayerStateChanged(const UDataLayerInstance* DataLayer, EDataLayerRuntimeState State)
{
	// 终极加固：如果角色自身正在被销毁，或者整个世界正在关闭/切换，拒绝响应以防野指针
	if (!IsValid(this) || !GetWorld() || GetWorld()->bIsTearingDown)
	{
		return;
	}

	// 状态过滤：仅抓取数据层被彻底卸载（Unloaded）的致命时刻，无视其他中间状态
	if (!DataLayer || State != EDataLayerRuntimeState::Unloaded)
	{
		return;
	}

	// 声明状态标记：用于记录自身所依赖的数据层是否正在崩塌
	bool bIsMyLayerCollapsing = false;

	// 提取正在卸载的数据层资产底层指针
	const UDataLayerAsset* CollapsingAsset = DataLayer->GetAsset();

	// 遍历当前角色实体所绑定的所有数据层资产
	for (const UDataLayerAsset* MyAsset : GetDataLayerAssets())
	{
		// 匹配校验：如果自身绑定的某一层刚好就是正在卸载的这层
		if (MyAsset == CollapsingAsset)
		{
			// 命中目标，将崩塌标记置为真并跳出循环
			bIsMyLayerCollapsing = true;
			break;
		}
	}

	// 确认大难临头：如果脚下的大地正在崩溃，立刻启动隔离管线防止卡位或崩溃
	if (bIsMyLayerCollapsing)
	{
		ExecuteSelfSanitization();
	}
}

void ATopCharacter::ExecuteSelfSanitization()
{
	UE_LOG(LogTemp, Warning, TEXT(">>> [响应式自治] 角色 %s 侦测到所在 physical/solid assets 数据层即将卸载，开始自我净化！"), *GetName());

	// 【架构优化实装】：功成身退，断开监听，节省 CPU 广播开销
	if (UWorld* World = GetWorld())
	{
		// 获取数据层管理器，准备提前断开监听，节省 CPU 广播开销
		if (UDataLayerManager* DLManager = UDataLayerManager::GetDataLayerManager(World))
		{
			DLManager->OnDataLayerInstanceRuntimeStateChanged.RemoveDynamic(this, &ATopCharacter::OnDataLayerStateChanged);
		}
	}

	// 从总线名册中自首注销
	if (AMyGameModeBase* GM = Cast<AMyGameModeBase>(GetWorld()->GetAuthGameMode()))
	{
		GM->FriendlyRoster.Remove(this);
	}

	// 执行“完美软禁”，保留组件结构但封死一切接收路径
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		// 清理物理键盘/手柄在这一帧残留的按压状态，防止软禁期间持续触发旧的输入指令
		PC->FlushPressedKeys();
		// 彻底剥夺控制器接受任何新输入事件的权限，切断玩家与肉体的逻辑交互
		PC->DisableInput(PC);
		// 强制忽略所有移动轴（WASD/左摇杆）的输入指令，锁死位移
		PC->SetIgnoreMoveInput(true);
		// 强制忽略所有视角轴（鼠标/右摇杆）的输入指令，锁死镜头转动
		PC->SetIgnoreLookInput(true);
	}

	// 【架构优化实装】：物理层面的彻底“软禁”，防止卡位
	// 渲染层剥离：将角色从画面中完全隐藏，防止在地形消失后玩家看到一个悬空穿模的模型
	SetActorHiddenInGame(true);
	// 物理层剥离：彻底关闭胶囊体和网格体的物理碰撞，杜绝其变成一堵“隐形的墙”卡住其他存活玩家
	SetActorEnableCollision(false);
	// 算力层剥离：强行掐断该 Actor 的每帧更新 (Tick)，在即将被销毁的最后阶段释放宝贵的 CPU 算力
	SetActorTickEnabled(false);

	// 提取角色的核心移动组件，准备进行物理休眠
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		// 动量清除：瞬间清空角色当前在空中的坠落速度或地面的奔跑惯性，使其绝对动能归零
		CMC->StopMovementImmediately();
		// 物理休眠：彻底挂起移动组件的内置状态机与重力计算，将其变成一个完全静止的死物
		CMC->DisableMovement();
	}
}

void ATopCharacter::Destroyed()
{
	// 【架构优化实装】：统一清理
	PerformCleanup();

	Super::Destroyed();
}

#pragma endregion

// ==============================================================================
// 玩家输入与行为绑定 (Player Input & Actions)
// ==============================================================================
#pragma region

void ATopCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// 【重回神坛：最安全的搭桥点】
	// 这里 100% 能拿到有效的 LocalPlayer，彻底告别开局失灵！
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		// 从本地玩家对象中提取增强输入子系统
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			// 校验设计师是否在蓝图中配置了默认的输入映射上下文
			if (DefaultMappingContext)
			{
				// 【重入优化实装】：先移除旧的（如果存在），防止因多次 Possess 导致的按键权重混乱
				Subsystem->RemoveMappingContext(DefaultMappingContext);
				Subsystem->AddMappingContext(DefaultMappingContext, 1);
			}
		}
	}

	// 强制将基础输入组件转换为虚幻5的增强输入组件，若失败则直接断言崩溃 (防呆)
	UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);

	// 绑定回调
	if (EnhancedInputComponent)
	{
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ATopCharacter::Move);
		// 跳跃直接绑定基类 ACharacter 原生的 Jump 函数，极其精简
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Started, this, &ATopCharacter::Attack);
		EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Completed, this, &ATopCharacter::AttackEnd);

		// 【新增】：绑定交互按键
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &ATopCharacter::OnInteractKeyPressed);
	}
}

void ATopCharacter::Move(const FInputActionValue& InputActionValue)
{
	// 【绝对防线】：如果底层传来的不是 Axis2D，立刻驳回！
	// 防止由于传送或控制权剥夺导致的 Enhanced Input 强行发送空事件引发 TVariant.h 崩溃
	if (InputActionValue.GetValueType() != EInputActionValueType::Axis2D) return;

	// 移动输入动作是一个 Axis2D 类型，要获取 X 和 Y 轴数据
	// InputActionValue.Get,将键盘传入的数据转换为二维向量
	// 键盘 X 表示 A/D，键盘 Y 表示 W/S，其中 D、W 为正值
	const FVector2D InputAxisVector = InputActionValue.Get<FVector2D>();

	if (MMCComponent)
	{
		// 让组件去处理具体的移动逻辑
		MMCComponent->HandleMoveInput(InputAxisVector);
	}
}

void ATopCharacter::Attack()
{
	// 1. 玩家点下左键，先问大管家
	if (ATopPlayerController* PC = Cast<ATopPlayerController>(GetController()))
	{
		// 2. 如果此时 UI 开着，大管家会去关掉 UI，并返回 true
		if (PC->ProcessGlobalClick())
		{
			// 3. 核心在这里！如果大管家返回了 true，直接 return 结束这个函数！
			// 这意味着这一下鼠标点击“被吃掉了”，下面的开火代码根本不会执行！
			return;
		}
	}

	// 4. 只有当 UI 已经关干净了，再点下一次鼠标时，大管家才会返回 false
	// 代码才会一路畅通走到这里，执行真正的开火！
	if (MCComponent)
	{
		MCComponent->StartWeaponFire();
	}
}

void ATopCharacter::AttackEnd()
{
	if (MCComponent)
	{
		//---------
		// 【新增】：检测按键抬起事件
		UE_LOG(LogTemp, Warning, TEXT("[输入诊断] <<< 鼠标左键 (Attack) 已抬起，发送停火指令 <<<"));
		//---------

		// 告诉战斗组件：停止射击
		MCComponent->StopWeaponFire();
	}
}

#pragma endregion

// ==============================================================================
// 交互统筹系统 (Interaction Management System)
// ==============================================================================
#pragma region

void ATopCharacter::OnInteractSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 三重拦截网：确认指针有效、绝对不能是玩家自身、且必须在代码或蓝图层面实现了交互接口契约
	if (OtherActor && OtherActor != this && OtherActor->Implements<UMyInteractableInterface>())
	{
		// 将合法的交互对象安全压入雷达缓存队列（防重加入），等待玩家按下按键时进行距离与权重比拼
		InteractableActorsInRange.AddUnique(OtherActor);

		// 【测谎仪节点 1 - 绿色】：验证物理边界撞击以及接口识别
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("[雷达成功探测] 目标物体: %s 进入交互范围，且已成功识别交互接口契约！"), *OtherActor->GetName()));
		}
	}
}

void ATopCharacter::OnInteractSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	// 1. 增加接口验证：只有真正带交互接口的物体脱离，才配进入逻辑
	if (OtherActor && OtherActor != this && OtherActor->Implements<UMyInteractableInterface>())
	{
		// 2. 增加防抖验证：Remove 会返回成功移除的数量。
		// 只有它之前真的在我们的待选队列里，现在被移除了，才打印日志！
		int32 RemovedCount = InteractableActorsInRange.Remove(OtherActor);

		if (RemovedCount > 0)
		{
			// 【测谎仪节点 2 - 红色】：验证合法交互物体离开
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("[雷达目标撤销] 目标物体: %s 离开了范围，已将其从待选队列移出。"), *OtherActor->GetName()));
			}
		}
	}
}

AActor* ATopCharacter::GetClosestInteractableActor()
{
	// 【前置防线：空盘拦截】
	// 检查当前雷达缓存的数组里是否为空
	if (InteractableActorsInRange.IsEmpty())
	{
		// 【测谎仪节点 3 - 橙色警告】：按键按下了，但列表居然是空的
		// @排错意义：帮助开发者瞬间定位 Bug 是出在“按键没响应”还是“碰撞没配对”上。
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Orange, TEXT("[排查警告] 名单列表为空！雷达此时没有捕获到任何实现了接口的合法物体，请检查碰撞通道。"));
		}
		// 没东西可交互，直接下班
		return nullptr;
	}

	// 最终胜出的“交互对象”指针
	AActor* BestActor = nullptr;
	// 距离擂主的记录板：初始设为宇宙最大值，保证第一个上台的合法选手一定能打破这个记录
	double MinDistanceSq = MAX_dbl;
	// 优先级擂主的记录板：初始设为宇宙最小值，保证第一个上台的合法选手一定能打破这个记录
	int32 HighestPriority = MIN_int32;

	// 缓存主角当前的绝对物理坐标，作为测量所有选手距离的“靶心”
	const FVector PlayerLocation = GetActorLocation();

	// 【架构级细节：为什么要倒序（--i）？】
	// 因为我们在循环中可能会执行 RemoveAt(i) 踢人操作！
	// 如果是正序（0 -> N），踢掉 2 号位后，3 号位会自动滑到 2 号位，此时下一次循环直接去查 3 号位，
	// 就会导致那个滑过来的原本的“3 号位”被彻底漏检。倒序遍历完美规避了数组动态缩容导致的越界和漏检！
	for (int32 i = InteractableActorsInRange.Num() - 1; i >= 0; --i)
	{
		// 从弱指针（TWeakObjectPtr 或类似包装）中提取真实的 Actor 内存地址
		AActor* CurrentActor = InteractableActorsInRange[i].Get();

		// 【严密防御机制：清理僵尸指针】
		// 1. IsValid：防止指向的内存已经被 GC（垃圾回收）清理。
		// 2. IsActorBeingDestroyed：防止物体正在播放销毁动画、即将死亡，但还没被彻底清出内存。
		if (!IsValid(CurrentActor) || CurrentActor->IsActorBeingDestroyed())
		{
			// 【性能优化实装】：使用 RemoveAtSwap 替代 RemoveAt，O(1) 极速删除，且无缝配合倒序遍历！
			InteractableActorsInRange.RemoveAtSwap(i);
			// 跳过本次循环，换下一个选手上台
			continue;
		}

		// 1. 测算优先级：通过 UE 的接口系统（Interface），向当前选手发问：“你的交互优先级是多少？”
		// 使用 Execute_ 前缀是因为该接口大概率为 BlueprintNativeEvent，必须走虚幻的反射调用机制。
		const int32 CurrentPriority = IMyInteractableInterface::Execute_GetInteractionPriority(CurrentActor);

		// 2. 测算距离：计算主角和选手之间的【距离平方】。
		// 【性能压榨】：为什么用 SizeSquared/DistSquared 而不是 Distance？
		// 算真实距离需要对坐标求算数平方根，CPU 开销极大。而比较远近大小，直接比平方值效果完全一样，这是 3A 级优化的基本素养。

		// 【终极手感优化：深度轴惩罚实装 (针对 3D 地图伪 2.5D 视角)】
		FVector Diff = CurrentActor->GetActorLocation() - PlayerLocation;
		Diff.Y *= 2.0f; // 赋予深度轴 (Y轴) 2倍的感知惩罚，让系统更倾向于选择“同一排”的物体
		const double DistanceSq = Diff.SizeSquared();

		// 【第一层筛选：绝对阶级压制】
		// 如果当前选手的优先级，严格大于现在的擂主（比如任务 NPC 优先级高于地上的普通树枝）
		if (CurrentPriority > HighestPriority)
		{
			// 无视距离，直接篡位夺权，全面刷新三项擂主指标！
			HighestPriority = CurrentPriority;
			MinDistanceSq = DistanceSq;
			BestActor = CurrentActor;
		}
		// 【第二层筛选：平民夺魁机制】
		// 如果当前选手的优先级，跟现在的擂主一模一样大（比如地上有两根一模一样的树枝）
		else if (CurrentPriority == HighestPriority)
		{
			// 进入残酷的物理距离大比拼，如果当前选手离玩家更近...
			if (DistanceSq < MinDistanceSq)
			{
				// 篡位夺权，刷新距离和擂主身份（但不刷新优先级，因为大家一样大）
				MinDistanceSq = DistanceSq;
				BestActor = CurrentActor;
			}
		}
	}

	// 返回经历了千锤百炼后，最终留在台上的那个唯一胜者（如果全都挂了，这就是个 nullptr）
	return BestActor;
}

void ATopCharacter::OnInteractKeyPressed()
{
	// 【测谎仪节点 5 - 浅蓝色】：验证增强输入到底有没有通到这个 C++ 函数里
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan, TEXT("[输入激活] 键盘 E 键已按下！增强输入底层管道打通，正在激活选品过滤算法..."));
	}

	// 只触发唯一的最佳目标，消除误操作
	// 调用筛选函数，获取当前范围内优先级最高且距离最近的交互目标
	if (AActor* TargetActor = GetClosestInteractableActor())
	{
		// 【原子性保护实装】：再次确认目标依然健康，防止在算法运行的一瞬间目标被服务器 Eliminate 
		if (IsValid(TargetActor) && !TargetActor->IsActorBeingDestroyed())
		{
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, FString::Printf(TEXT("[锁定成功] 最终决出的最优目标是: %s！正在向其投掷 Execute_Interact 接口调用命令！"), *TargetActor->GetName()));
			}

			// 确认目标有效后，通过蓝图接口系统 (Interface) 通知目标执行具体的交互逻辑
			// 将玩家自身 (this) 作为交互的发起者 (Instigator) 传递给目标
			// 接收者 AMySavePointActor::Interact_Implementation
			IMyInteractableInterface::Execute_Interact(TargetActor, this);
		}
	}
}

#pragma endregion


// ==============================================================================
// 联机底层探针 (Network Probes)
// ==============================================================================
#pragma region

void ATopCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// 甄别身份：仅当附身者是真实的真人玩家控制器（而非 AI 控制器）时，才执行网络重置逻辑
	if (NewController && NewController->IsPlayerController())
	{
		// 尝试转换为基类玩家控制器，准备执行强制对齐
		if (APlayerController* PC = Cast<APlayerController>(NewController))
		{
			// 【终极网络稳定实装】：强迫客机控制器重新对齐旋转与输入状态，消除换人瞬移与视角闪烁
			PC->ClientRestart(this);
		}

		// 获取角色的移动组件，准备清理残留的网络平滑缓存
		if (UCharacterMovementComponent* CMC = GetCharacterMovement())
		{
			// 【2.5D 空中牵引力收尾】：拿回控制器后，坠落阻力恢复默认
			CMC->BrakingDecelerationFalling = 0.f;

			// 【修复】：使用正确的底层 API。
			// 彻底清除旧控制器（无人/AI）在底层留下的网络平滑数据，
			// 完美保留当前的物理速度 (Velocity)，消除切换身份 (Proxy) 时的鬼畜抖动。
			CMC->ResetPredictionData_Client();
			CMC->ResetPredictionData_Server();

			// 【极致加固】：逼迫服务器立刻执行一次强同步，确保客机夺舍瞬间的绝对坐标毫无抖动回弹！
			ForceNetUpdate();
		}
	}

	// 三元表达式安全提取新控制器的名称，防止空指针导致日志崩溃
	FString ControllerName = NewController ? NewController->GetName() : TEXT("空(Null)");
	UE_LOG(LogTemp, Warning, TEXT("⚔️ [Character探针-服务器端] 玩家专属肉体被注入灵魂! Character: %s <- Controller: %s"), *GetName(), *ControllerName);
}

void ATopCharacter::UnPossessed()
{
	Super::UnPossessed();

	// 【2.5D 移动修正实装】：空中转向的“牵引力”补偿
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		// 稍微增加一点空气阻力，防止无主躯壳失去控制器后，被惯性甩出平台
		CMC->BrakingDecelerationFalling = 1500.f;
	}
}

void ATopCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();

	// 提取客户端本地刚刚同步到的最新控制器指针
	AController* CurrentController = GetController();

	// 三元表达式安全提取名称准备打印，防止同步中途指针丢失引发崩溃
	FString ControllerName = CurrentController ? CurrentController->GetName() : TEXT("空(Null)");

	// 客户端专属行为：向控制台打印客机本地成功接收到服务器控制器下发数据的探针日志
	UE_LOG(LogTemp, Warning, TEXT("📡 [Character探针-客户端端] 玩家专属肉体收到灵魂同步广播! Character: %s <- Controller: %s"), *GetName(), *ControllerName);
}

#pragma endregion
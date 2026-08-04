#include "MyOnline/MyLobbySubsystem.h"
#include "MyOnline/EOSTestInjector.h" 
#include "Online/OnlineServices.h"
#include "Online/Auth.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Online/OnlineAsyncOpHandle.h" // 提供异步句柄的完整定义以支持 .OnComplete()

// 【新增】：引入路由资产，解析软引用
#include "MapTravel/DataAsset/TeleportRoute.h" 
// 【新增】：跨系统呼叫大一统传送管线
#include "MapTravel/MyMapTravelSubsystem.h"

// 在文件顶部引入你的新数据资产
#include "MyOnline/LobbyConfigAsset.h"

#include "Game/MyGameInstance.h"


// ==============================================================================
// 核心生命周期与组件 (Core Lifecycle & Components)
// ==============================================================================
#pragma region

void UMyLobbySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	// 调用父类的初始化逻辑，确保基础子系统功能正常挂载
	Super::Initialize(Collection);
}

void UMyLobbySubsystem::Deinitialize()
{
	// 引擎关闭时：执行清理，防止在 Epic 后端留下“僵尸房间”
	// 检查当前缓存的大厅 ID 是否有效，有效则说明当前正处于大厅中
	if (CurrentLobbyId.IsValid())
	{
		// 引入 OSSv2 命名空间，简化后续代码
		using namespace UE::Online;

		// 尝试获取默认的在线服务模块接口
		if (IOnlineServicesPtr OnlineServices = GetServices(EOnlineServices::Default))
		{
			// 从在线服务中提取大厅 (Lobbies) 接口指针
			if (ILobbiesPtr LobbiesInterface = OnlineServices->GetLobbiesInterface())
			{
				// 从在线服务中提取身份验证 (Auth) 接口指针
				if (IAuthPtr AuthInterface = OnlineServices->GetAuthInterface())
				{
					// 获取当前本地主玩家的物理设备映射 ID (处理多手柄/键鼠输入绑定)
					FPlatformUserId PlatformUserId = IPlatformInputDeviceMapper::Get().GetPrimaryPlatformUser();

					// 【核心修正 1】：在引擎开始销毁物理设备映射器时，增加有效性拦截
					// 确保在引擎彻底销毁前，该设备 ID 依然合法
					if (PlatformUserId.IsValid())
					{
						// 通过物理设备 ID 向 Auth 接口查询对应的在线账号信息
						if (auto UserResult = AuthInterface->GetLocalOnlineUserByPlatformUserId({ PlatformUserId }); UserResult.IsOk())
						{
							// 组装离开大厅 (LeaveLobby) 所需的参数结构体
							FLeaveLobby::Params LeaveParams;
							// 填入刚刚查询到的本地玩家 AccountId
							LeaveParams.LocalAccountId = UserResult.GetOkValue().AccountInfo->AccountId;
							// 填入需要退出的目标大厅 ID
							LeaveParams.LobbyId = CurrentLobbyId;

							// 【加固 5 + 进阶加固 A】：极限防崩溃！
							// 此时 Subsystem 即将被系统无情抹除，绝对不能在异步回调里使用 this 或 WeakSelf。
							// 改为纯静态 Lambda，保证底层有足够的时间窗口释放资源，同时不触碰任何已死亡的类成员！
							// 发起异步的离开大厅网络请求
							LobbiesInterface->LeaveLobby(MoveTemp(LeaveParams))
								.OnComplete([](const TOnlineResult<FLeaveLobby>& Result)
									{
										// 当回调触发时，只做静态日志打印，绝不访问外部对象的内存
										if (Result.IsOk())
										{
											UE_LOG(LogTemp, Warning, TEXT("引擎销毁前：成功从 Epic 后端释放并退出大厅。"));
										}
									});
						}
					}
				}
			}
		}
	}

	// 资源释放完毕后，调用父类的反初始化完成最后销毁
	Super::Deinitialize();
}

#pragma endregion

// ==============================================================================
// 现代化联机大厅 (OSSv2 Lobbies)
// ==============================================================================
#pragma region

void UMyLobbySubsystem::CreateEOSLobby(ULobbyConfigAsset* LobbyConfig)
{
	// 引入 OSSv2 命名空间
	using namespace UE::Online;

	// 打印硬核雷达日志，标记建房流程开始
	INJECTOR_LOG(Warning, TEXT("========== [硬核雷达] 开始诊断 CreateEOSLobby =========="));

	// 获取默认在线服务句柄
	IOnlineServicesPtr OnlineServices = GetServices(EOnlineServices::Default);
	// 拦截：如果在线服务未启动，直接终止流程
	if (!OnlineServices.IsValid())
	{
		INJECTOR_LOG(Error, TEXT("[诊断结果] 失败: OnlineServices 接口无效！"));
		return;
	}

	// 获取大厅功能接口
	ILobbiesPtr LobbiesInterface = OnlineServices->GetLobbiesInterface();
	// 获取身份验证功能接口
	IAuthPtr AuthInterface = OnlineServices->GetAuthInterface();
	// 拦截：如果任何一个核心接口获取失败，说明插件配置有问题或未加载
	if (!LobbiesInterface.IsValid() || !AuthInterface.IsValid())
	{
		INJECTOR_LOG(Error, TEXT("[诊断结果] 失败: Lobbies 或 Auth 接口获取为空！"));
		return;
	}

	// 获取本地账号 ID (OSSv2 异步获取)
	// 明确是谁（哪个物理手柄/键盘）在发起建房请求
	FPlatformUserId PlatformUserId = IPlatformInputDeviceMapper::Get().GetPrimaryPlatformUser();
	// 打印当前操作者的底层内部映射 ID 以供调试
	INJECTOR_LOG(Warning, TEXT("[雷达] 当前操作者的 PlatformUserId 内部ID: %d"), PlatformUserId.GetInternalId());

	// 同步查询该物理设备关联的合法在线用户身份
	TOnlineResult<FAuthGetLocalOnlineUserByPlatformUserId> UserResult = AuthInterface->GetLocalOnlineUserByPlatformUserId({ PlatformUserId });

	// 如果查询失败，说明该设备并没有完成 OSSv2 登录流程（或底层 C-API 同步失败）
	if (!UserResult.IsOk())
	{
		// 探针：抓取具体的错误详情
		FString ErrorStr = UserResult.GetErrorValue().GetLogString();
		INJECTOR_LOG(Error, TEXT("❌ [致命拦截] 获取 LocalUser 失败，大厅流程被强制终止！"));
		INJECTOR_LOG(Error, TEXT("❌ [底层原因] %s"), *ErrorStr);
		INJECTOR_LOG(Warning, TEXT("=================================================================="));

		// 拦截：没有合法身份，绝对不能向 Epic 发起建房请求
		INJECTOR_LOG(Error, TEXT("创建大厅失败：尚未完成身份认证！"));
		return;
	}
	// 从查询结果中安全提取玩家的 AccountId 句柄
	FAccountId LocalAccountId = UserResult.GetOkValue().AccountInfo->AccountId;
	// 打印获取成功的纯净 ID 字符串
	INJECTOR_LOG(Warning, TEXT("[雷达] 身份校验通过！成功拿到 AccountId: %s"), *ToLogString(LocalAccountId));


	// ==============================================================================
	// 【参数组装】：检查是否传入配置，动态应用数据资产或默认降级
	// ==============================================================================
	// 初始化建房参数结构体
	FCreateLobby::Params Params;
	// 指定房主的账号 ID
	Params.LocalAccountId = LocalAccountId;
	// 设置大厅的本地调试代号（不一定在广域网显示，用于本地索引）
	Params.LocalName = FName(TEXT("MyCoopLobby"));

	// 检查是否从蓝图/UI 传入了有效的大厅配置资产
	if (LobbyConfig)
	{
		// 存在有效配置：提取数据资产中的参数
		// 使用资产配置的大厅 Schema（大厅结构定义）
		Params.SchemaId = LobbyConfig->SchemaId;
		// 是否开启富文本在线状态（Presence）
		Params.bPresenceEnabled = LobbyConfig->bPresenceEnabled;
		// 设置大厅允许的最大玩家数量
		Params.MaxMembers = LobbyConfig->MaxMembers;

		// 【修复】：严格对应 OSSv2 的合法枚举
		// 将我们自定义的蓝图枚举，安全转换为 Epic 底层要求的权限枚举
		switch (LobbyConfig->JoinPolicy)
		{
		case EMyLobbyJoinPolicy::PublicNotAdvertised:
			// 公开但不广播（知道ID才能进）
			Params.JoinPolicy = ELobbyJoinPolicy::PublicNotAdvertised;
			break;
		case EMyLobbyJoinPolicy::InvitationOnly:
			// 仅限邀请
			Params.JoinPolicy = ELobbyJoinPolicy::InvitationOnly;
			break;
		case EMyLobbyJoinPolicy::PublicAdvertised:
		default:
			// 完全公开，可在广域网搜到
			Params.JoinPolicy = ELobbyJoinPolicy::PublicAdvertised;
			break;
		}

		// 使用数据资产中的自定义属性
		// 写入自定义键值对，后续客机可以用 "MyRoomType" 作为条件进行搜索
		Params.Attributes.Add(TEXT("MyRoomType"), FSchemaVariant(LobbyConfig->DefaultRoomType));
		// 打印最终配置的人数，方便核对
		INJECTOR_LOG(Warning, TEXT("正在按照数据资产配置创建隐形集结号... 人数: %d"), Params.MaxMembers);
	}
	else
	{
		// 无配置传入兜底：使用默认安全参数
		// 默认的大厅 Schema
		Params.SchemaId = FName(TEXT("GameLobby"));
		// 默认开启 Presence
		Params.bPresenceEnabled = true;
		// 默认限定 4 人联机
		Params.MaxMembers = 4;
		// 默认权限为完全公开
		Params.JoinPolicy = ELobbyJoinPolicy::PublicAdvertised;

		// 使用原有的默认属性兜底
		// 写入默认的搜索属性
		Params.Attributes.Add(TEXT("GameMode"), FSchemaVariant(FString(TEXT("Coop"))));
		// 打印警告日志，提示正在使用兜底策略
		INJECTOR_LOG(Warning, TEXT("未检测到配置资产，正在使用默认兜底参数创建隐形集结号..."));
	}

	// 内存安全拦截防线
	// 捕获当前子系统的弱指针，防止在网络回调时子系统已被销毁
	TWeakObjectPtr<UMyLobbySubsystem> WeakSelf(this);

	// 正式向 Epic 服务器发送建房网络请求
	LobbiesInterface->CreateLobby(MoveTemp(Params))
		.OnComplete(this, [WeakSelf](const TOnlineResult<FCreateLobby>& Result)
			{
				// 回调第一步：检查自己是否还活着，死了直接退出
				if (!WeakSelf.IsValid()) return;

				// 如果 Epic 服务器返回建房成功
				if (Result.IsOk())
				{
					// 【核心解绑】：建房成功后，仅仅是保存大厅 ID，不再执行任何会引发冲突的 ServerTravel 操作！
					// 将云端生成的大厅唯一 ID 缓存到本地
					WeakSelf->CurrentLobbyId = Result.GetOkValue().Lobby->LobbyId;
					// 打印成功日志，大厅此时已处于“隐形集结”状态
					INJECTOR_LOG(Warning, TEXT("✅ 集结号吹响成功，房间纯数据已建立: %s"), *ToLogString(WeakSelf->CurrentLobbyId));
				}
				else
				{
					// 建房失败：抓取并打印底层的具体错误信息（如断网、权限不足）
					INJECTOR_LOG(Error, TEXT("❌ [Epic服务器拒绝] 建房失败: %s"), *Result.GetErrorValue().GetLogString());
					INJECTOR_LOG(Error, TEXT("❌ 失败: %s"), *Result.GetErrorValue().GetLogString());
				}
				// 打印分隔线，结束此次请求生命周期的日志
				INJECTOR_LOG(Warning, TEXT("=================================================================="));
			});
}

void UMyLobbySubsystem::StartEOSGame(UTeleportRoute* TargetRoute)
{
	// 基础防线：拦截未配置的空路由资产
	// 防止 UI 层手误传进来一个空指针导致崩溃
	if (!TargetRoute)
	{
		INJECTOR_LOG(Error, TEXT("❌ [安全拦截] 传入的路由资产为空！"));
		return;
	}

	// 校验路由资产内部是否确实配置了跨图的目的地（软引用是否为空）
	// 防止资产虽然存在，但策划忘记在资产里配置目标关卡
	if (TargetRoute->TargetMap.IsNull())
	{
		INJECTOR_LOG(Error, TEXT("❌ [致命错误] 路由资产 [%s] 中没有配置任何目标地图！"), *TargetRoute->GetName());
		return;
	}

	// 尝试获取当前所处的物理世界 (UWorld)
	if (UWorld* World = GetWorld())
	{
		// ==============================================================================
		// 【核心修复：给房主补发车票！】
		// 不管是踩传送门还是大厅开房，只要是带着目的地的路由，必须把票交给大管家！
		// 否则落地后大管家会把房主当成没票的客机处理！
		// 
		// 【加固 2 终极警告】：请务必确保 UMyGameInstance.h 中的 PendingTravelRoute 
		// 带有 UPROPERTY() 宏，并且显式初始化为 nullptr！否则在 ServerTravel 切图瞬间会被 GC 碾碎！
		// ==============================================================================
		// 获取与游戏进程同生共死的 GameInstance
		if (UMyGameInstance* GI = World->GetGameInstance<UMyGameInstance>())
		{
			// 将当前的车票（路由数据）强行绑定到大管家身上，对抗 ServerTravel 的世界销毁
			GI->PendingTravelRoute = TargetRoute;
		}

		// 【极致复用】：完全复用架构里的软引用萃取逻辑
		// 从软引用萃取真实地图包短名
		// 这一步避免了硬编码字符串，将资源软引用安全解析为引擎路由认得的短名
		FName TargetMapName = FName(*TargetRoute->TargetMap.GetAssetName());

		// 打印发车雷达日志
		INJECTOR_LOG(Warning, TEXT("========== [硬核雷达] 房主准备开启 P2P 隧道并流送地图 =========="));
		// 明确打印本次发车的路由代号以及它对应的真实地图名字
		INJECTOR_LOG(Warning, TEXT("路由名: [%s] -> 目标地图: %s"), *TargetRoute->GetName(), *TargetMapName.ToString());

		// 呼叫大世界漫游管线！
		// 尝试获取专门负责漫游过图的子系统
		if (UMyMapTravelSubsystem* TravelSub = World->GetSubsystem<UMyMapTravelSubsystem>())
		{
			// 将拉黑幕、点穴、断开输入、关无缝漫游、加 ?listen 等所有脏活全部交给传送总管！
			// 把目的地名字递交过去，由专线处理 UI 和网络底层命令
			TravelSub->ExecuteHostTravel(TargetMapName);
		}
		else
		{
			// 极端异常兜底
			// 如果传送管线子系统挂了，手动拼接 URL 强制发车
			FString TravelURL = FString::Printf(TEXT("%s?listen"), *TargetMapName.ToString());
			// 执行虚幻原生的服务端切图逻辑，开启 listen 监听客机连接
			World->ServerTravel(TravelURL, true);
		}
	}
}

void UMyLobbySubsystem::FindEOSLobbies()
{
	// 引入 OSSv2 命名空间
	using namespace UE::Online;

	// 获取默认在线服务句柄
	IOnlineServicesPtr OnlineServices = GetServices(EOnlineServices::Default);
	// 如果服务未启动，直接返回
	if (!OnlineServices.IsValid()) return;

	// 提取大厅接口
	ILobbiesPtr LobbiesInterface = OnlineServices->GetLobbiesInterface();
	// 提取认证接口
	IAuthPtr AuthInterface = OnlineServices->GetAuthInterface();

	// 识别正在操作搜房动作的本地物理设备
	FPlatformUserId PlatformUserId = IPlatformInputDeviceMapper::Get().GetPrimaryPlatformUser();
	// 查询该设备对应的在线身份
	TOnlineResult<FAuthGetLocalOnlineUserByPlatformUserId> UserResult = AuthInterface->GetLocalOnlineUserByPlatformUserId({ PlatformUserId });
	// 如果没有合法身份，无权搜房，直接退出
	if (!UserResult.IsOk()) return;

	// 安全提取执行搜索任务的 AccountId
	FAccountId LocalAccountId = UserResult.GetOkValue().AccountInfo->AccountId;

	// 组装大厅搜索参数结构体
	FFindLobbies::Params Params;
	// 指定是谁发起的搜索
	Params.LocalAccountId = LocalAccountId;
	// 限定单次最大返回的房间数量为 10，防止内存或 UI 溢出
	Params.MaxResults = 10;

	// 【修复 1】：FFindLobbiesSearchFilter -> FFindLobbySearchFilter
	// 构建单一的搜索过滤条件
	FFindLobbySearchFilter Filter;
	// 指定要匹配的键名
	Filter.AttributeName = TEXT("GameMode");
	// 指定比较操作符为严格相等 (Equals)
	Filter.ComparisonOp = ESchemaAttributeComparisonOp::Equals;
	// 指定要匹配的值（需与建房时的兜底或资产配置一致）
	Filter.ComparisonValue = FSchemaVariant(FString(TEXT("Coop")));
	// 将该条件加入到搜索参数的过滤器数组中
	Params.Filters.Add(Filter);

	// ==============================================================================
	// 【进阶加固 D 预留】：空房间过滤与精细化分页
	// TODO: 后续如果用户量增大，可在此利用 LobbyConfigAsset 传入的 BucketId 进行强过滤。
	// 也可增加自定义属性如 "AvailableSlots > 0" 的二次 Filter，从源头掐断满员或空废房间。
	// ==============================================================================

	// 打印搜房日志
	INJECTOR_LOG(Warning, TEXT("正在广域网搜索主机的集结号..."));

	// 捕获弱指针，防止回调时类已死亡
	TWeakObjectPtr<UMyLobbySubsystem> WeakSelf(this);

	// 向 Epic 服务器下发搜房请求
	LobbiesInterface->FindLobbies(MoveTemp(Params))
		.OnComplete(this, [WeakSelf, LocalAccountId](const TOnlineResult<FFindLobbies>& Result) // 捕获 LocalAccountId 用于过滤
			{
				// 检查子系统对象生命周期
				if (!WeakSelf.IsValid()) return;

				// 如果云端搜索成功返回
				if (Result.IsOk())
				{
					// 提取搜索结果列表 (Lobbies) 的常量引用
					const TArray<TSharedRef<const FLobby>>& FoundLobbies = Result.GetOkValue().Lobbies;
					// 设置一个标记位，用于记录是否找到了真正可加入的有效房间
					bool bFoundValidLobby = false;

					// 【加固 3】：搜索结果过滤防御，源头拦截自我房间，防止玩家误触导致本地网络端口自爆
					// 遍历服务器返回的所有匹配房间
					for (const auto& Lobby : FoundLobbies)
					{
						// 如果当前房间的房主 ID 不等于搜索者自己的 ID
						if (Lobby->OwnerAccountId != LocalAccountId)
						{
							INJECTOR_LOG(Warning, TEXT("🔍 找到了有效的他人集结号！自动执行 P2P 穿插..."));
							// 自动向这个有效的他人房间发起加入请求
							WeakSelf->JoinEOSLobby(Lobby->LobbyId);
							// 标记为已找到有效房间
							bFoundValidLobby = true;
							// 找到一个就立刻跳出循环（避免同时加入多个）
							break;
						}
					}

					// 如果遍历了一圈都没有发现别人的房间
					if (!bFoundValidLobby)
					{
						// 如果总数大于0，说明搜到了房间，但全被过滤了（全是自己的）
						if (FoundLobbies.Num() > 0)
						{
							INJECTOR_LOG(Warning, TEXT("搜寻完成，找到了 %d 个房间，但全都是自己建立的房间，已安全过滤。"), FoundLobbies.Num());
						}
						// 否则就是真没人在开房
						else
						{
							INJECTOR_LOG(Warning, TEXT("搜寻完成，广域网上没有找到任何匹配的集结号。"));
						}
					}
				}
				else
				{
					// 打印 Epic 抛出的搜索报错详情
					INJECTOR_LOG(Error, TEXT("❌ 搜寻失败: %s"), *Result.GetErrorValue().GetLogString());
				}
			});
}

void UMyLobbySubsystem::JoinEOSLobby(const UE::Online::FLobbyId& LobbyIdToJoin)
{
	// 引入 OSSv2 命名空间
	using namespace UE::Online;

	// 获取默认在线服务句柄
	IOnlineServicesPtr OnlineServices = GetServices(EOnlineServices::Default);
	// 如果不可用则返回
	if (!OnlineServices.IsValid()) return;

	// 提取大厅和身份验证接口
	ILobbiesPtr LobbiesInterface = OnlineServices->GetLobbiesInterface();
	IAuthPtr AuthInterface = OnlineServices->GetAuthInterface();

	// 明确请求发起者的物理设备
	FPlatformUserId PlatformUserId = IPlatformInputDeviceMapper::Get().GetPrimaryPlatformUser();
	// 查询其在线身份
	TOnlineResult<FAuthGetLocalOnlineUserByPlatformUserId> UserResult = AuthInterface->GetLocalOnlineUserByPlatformUserId({ PlatformUserId });
	// 如果无合法身份则截断
	if (!UserResult.IsOk()) return;

	// 拿到即将加入大厅的客机 ID
	FAccountId LocalAccountId = UserResult.GetOkValue().AccountInfo->AccountId;

	// 组装加入大厅的参数
	FJoinLobby::Params Params;
	// 指定加入者账号
	Params.LocalAccountId = LocalAccountId;
	// 填入搜到的或被邀请的大厅 ID
	Params.LobbyId = LobbyIdToJoin;
	// 设置客机的 Presence 状态（在线可见性）
	Params.bPresenceEnabled = true;

	// 打印开始加入的日志
	INJECTOR_LOG(Warning, TEXT("正在申请加入大厅..."));

	// 声明弱指针保护生命周期
	TWeakObjectPtr<UMyLobbySubsystem> WeakSelf(this);

	// 正式下发加入请求
	LobbiesInterface->JoinLobby(MoveTemp(Params))
		.OnComplete(this, [WeakSelf, LocalAccountId](const TOnlineResult<FJoinLobby>& Result)
			{
				// 验证类存活状态
				if (!WeakSelf.IsValid()) return;

				// 如果 Epic 服务器同意加入
				if (Result.IsOk())
				{
					// 获取加入成功后返回的大厅数据快照
					TSharedPtr<const FLobby> JoinedLobby = Result.GetOkValue().Lobby;
					// 防御性拦截空指针
					if (!JoinedLobby.IsValid()) return;

					// 记录当前客机所处的大厅 ID
					WeakSelf->CurrentLobbyId = JoinedLobby->LobbyId;

					// 提取大厅房主（Host）的 AccountId
					// 这是后续 P2P 穿透连接目标的关键凭证
					FAccountId HostId = JoinedLobby->OwnerAccountId;

					// ==============================================================================
					// 【核心防御】：物理斩断自我连接导致的 NetDriver 自爆！
					// ==============================================================================
					// 终极拦截：虽然搜索时过滤了，但在好友邀请等其他途径中，依然要严防自己加自己
					if (HostId == LocalAccountId)
					{
						INJECTOR_LOG(Error, TEXT("❌ [安全拦截] 灾难规避！检测到试图加入自己创建的房间。"));
						INJECTOR_LOG(Error, TEXT("❌ 操作已被系统底层物理隔离，以防止网络引擎崩溃！"));
						return; // 立即打断，绝不执行后续的 ClientTravel
					}

					// ==============================================================================
					// 【进阶加固 C 预留】：加入大厅后的属性预检
					// TODO: 如果游戏分为了 UI 选人界面与真实战斗地图两部分，可以在这里拉取大厅属性
					// (如 JoinedLobby->Attributes["RoomState"])，以此判断房主是否真正下发了发车指令，
					// 而不是刚进大厅就无脑调 ExecuteClientJoin 导致连接失败。
					// ==============================================================================

					// ==============================================================================
					// 【终极解析】：剥离 OSSv2 的包装，提取纯净 32 位 PUID 供 URL 解析器识别
					// 这段实战派切片逻辑绝对正确，完美绕过了 OSSv2 句柄的不透明性！
					// ==============================================================================
					// 利用引擎提供的 LogString 强行将黑盒对象暴露为字符串
					FString DebugStr = ToLogString(HostId); // 格式如: Epic:2 (0002d670ec754ccf9d82c9dcb609a689)
					FString RawPUID;

					int32 StartIdx, EndIdx;
					// 在字符串中寻找括号的位置
					if (DebugStr.FindChar('(', StartIdx) && DebugStr.FindChar(')', EndIdx))
					{
						// 精准提取出括号里的 32 位特征码 (即纯净的 Epic 账号 ID)
						RawPUID = DebugStr.Mid(StartIdx + 1, EndIdx - StartIdx - 1);
					}
					else
					{
						// 兜底防御：如果格式突变，直接原样赋值，交由底层网络驱动自己报错
						RawPUID = DebugStr;
					}

					// ==============================================================================
					// 【终极解析】：原汁原味的 FURL 欺骗战术，利用 IPv6 括号骗过引擎协议解析！
					// ==============================================================================
					// 使用 [EOS:%s] 格式：利用 FURL IPv6 的解析盲区，隐藏 EOS 协议头，强迫引擎走默认 GameNetDriver
					FString ConnectString = FString::Printf(TEXT("[EOS:%s]"), *RawPUID);

					// 打印组装好的穿透地址
					INJECTOR_LOG(Warning, TEXT("🚀 P2P 隧道提取解析成功，准备发起【初始网络连接】: %s"), *ConnectString);

					// 呼叫大世界漫游管线！
					// 将连接字符串交给专业的切图子系统
					if (UMyMapTravelSubsystem* TravelSub = WeakSelf->GetWorld()->GetSubsystem<UMyMapTravelSubsystem>())
					{
						INJECTOR_LOG(Warning, TEXT("⚡ 正在把控制权移交给大一统传送管线进行 P2P 穿透..."));

						// 直接呼叫管线的新专线：客机接线加入！享有完美的黑屏掩护和输入剥夺
						// 让漫游管线接管 UI 与 ClientTravel，实现无感体验
						TravelSub->ExecuteClientJoin(ConnectString);
					}
					else
					{
						// 极端异常兜底
						// 如果漫游管线子系统不可用，直接命令玩家控制器裸连目标房主
						if (APlayerController* PC = WeakSelf->GetWorld()->GetFirstPlayerController())
						{
							PC->ClientTravel(ConnectString, ETravelType::TRAVEL_Absolute);
						}
					}
				}
				else
				{
					// 打印 Epic 抛出的加入失败错误日志
					INJECTOR_LOG(Error, TEXT("❌ 加入大厅失败: %s"), *Result.GetErrorValue().GetLogString());
				}
			});
}

#pragma endregion
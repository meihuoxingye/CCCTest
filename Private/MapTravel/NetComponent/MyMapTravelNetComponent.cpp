// Fill out your copyright notice in the Description page of Project Settings.


#include "MapTravel/NetComponent/MyMapTravelNetComponent.h"
#include "Net/UnrealNetwork.h"
#include "Game/MyGameInstance.h"

// ==============================================================================
// 核心生命周期与组件 (Core Lifecycle & Components)
// ==============================================================================
#pragma region

UMyMapTravelNetComponent::UMyMapTravelNetComponent()
{
	// 必须开启组件复制，否则无法作为 RPC 通信的合法载体
	SetIsReplicatedByDefault(true);
}

void UMyMapTravelNetComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 注册状态机变量参与网络复制，确保持续追踪该玩家的就绪状态
	DOREPLIFETIME(UMyMapTravelNetComponent, CurrentState);
}

#pragma endregion


// ==============================================================================
// 核心网络握手管线 (Core Network Handshake)
// ==============================================================================
#pragma region

void UMyMapTravelNetComponent::Server_InitiateTransition(const FTransform& TargetTransform)
{
	// 安全校验：只有拥有权威的服务器（房主）才能对全局管线发号施令
	if (!GetOwner()->HasAuthority()) return;

	// 状态机切换为：正在等待客户端 UI 响应
	CurrentState = ETravelSyncState::WaitingForClientUI;

	UE_LOG(LogTemp, Warning, TEXT("📡 [Server -> Client] 发令：同图转场开始！通知客机实体 [%s] 立即拉起黑幕..."), *GetOwner()->GetName());

	// 下发 RPC 强制指令给该组件挂载的特定客户端
	Client_HandleTransitionRequest(TargetTransform);
}

void UMyMapTravelNetComponent::Client_HandleTransitionRequest_Implementation(FTransform TargetTransform)
{
	// 此处运行在客机本地（也包含主机自己的本地客户端）
	UE_LOG(LogTemp, Warning, TEXT("🖥️ [Client] 收到！客机实体 [%s] 正在拉起同图黑幕，交出物理控制权..."), *GetOwner()->GetName());

	// 触发本地大管家拉起黑幕，注意：传入 nullptr 作为 Actor，
	// 彻底声明本地不再负责物理位移，全部交由服务器统筹！
	if (UMyGameInstance* GI = GetWorld()->GetGameInstance<UMyGameInstance>())
	{
		GI->ExecuteSameMapTransition(nullptr, TargetTransform);
	}
}

void UMyMapTravelNetComponent::Server_NotifyClientReady_Implementation()
{
	// 客户端 UI 完全黑屏后，调用此 RPC 通知服务器
	CurrentState = ETravelSyncState::Ready;

	UE_LOG(LogTemp, Warning, TEXT("✅ [Client -> Server] 汇报：玩家实体 [%s] 客户端已 100%% 黑屏，物理剥离完毕，进入待命状态。"), *GetOwner()->GetName());
}

void UMyMapTravelNetComponent::Client_NotifyPhysicalTeleportDone_Implementation()
{
	// 收到服务器“全员已完成物理折叠对齐”的解封令
	UE_LOG(LogTemp, Warning, TEXT("🖥️ [Client] 解封！客机实体 [%s] 收到服务器物理对齐完毕指令，准备亮屏..."), *GetOwner()->GetName());

	// 此时直接唤醒大管家执行下半场：数据层真实切换、卸载黑幕、亮屏退场
	if (UMyGameInstance* GI = GetWorld()->GetGameInstance<UMyGameInstance>())
	{
		GI->FinalizeSameMapTransition();
	}
}

#pragma endregion


// ==============================================================================
// 跨地图专属握手管线 (Cross-Map Handshake)
// ==============================================================================
#pragma region

void UMyMapTravelNetComponent::Server_InitiateCrossMapTransition(FName TargetMapName)
{
	// 安全校验：只有拥有权威的服务器（房主）才能对全局管线发号施令
	if (!GetOwner()->HasAuthority()) return;

	// 状态机切换为：正在等待客户端 UI 响应
	CurrentState = ETravelSyncState::WaitingForClientUI;

	UE_LOG(LogTemp, Warning, TEXT("📡 [Server -> Client] 发令：跨图转场起航！通知客机实体 [%s] 遮蔽视线，目标地图: [%s]..."), *GetOwner()->GetName(), *TargetMapName.ToString());

	// 【调用原名】：触发 RPC 发送时，必须调用不带后缀的原名！引擎会将其拦截并通过网线发送给客机。
	Client_HandleCrossMapRequest(TargetMapName);
}

// 【实装必须带后缀】：在 CPP 中编写具体的业务逻辑时，函数名必须强制加上 _Implementation 后缀！
// 否则 C++ 编译器会报“函数重定义”错误，因为原名已被虚幻底层的 .generated.h 物理占用。
void UMyMapTravelNetComponent::Client_HandleCrossMapRequest_Implementation(FName TargetMapName)
{
	// 此处运行在客机本地（也包含主机自己的本地客户端）
	UE_LOG(LogTemp, Warning, TEXT("🖥️ [Client] 收到！客机实体 [%s] 正在拉起跨图黑幕，准备迎接世界撕裂..."), *GetOwner()->GetName());

	if (UMyGameInstance* GI = GetWorld()->GetGameInstance<UMyGameInstance>())
	{
		// 跨图表现层统筹：强行注入目标地图，提取配置并拉起黑幕
		GI->PendingTargetMapName = TargetMapName;
		FMapTransitionConfig Config = GI->GetMapTransitionConfig(TargetMapName);
		GI->PlayScreenOffPhaseUI(Config.ScreenOffUIClass, Config.ScreenOffDuration);

		// 汇报服务器：客机本地黑幕已遮蔽视线，准备迎接 ServerTravel 的空间撕裂！
		Server_NotifyClientReady();
	}
}

#pragma endregion
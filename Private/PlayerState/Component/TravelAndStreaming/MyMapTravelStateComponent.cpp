#include "PlayerState/Component/TravelAndStreaming/MyMapTravelStateComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
// 引入大一统传送子系统，作为令牌翻转时的实际“执行导演”
#include "MapTravel/MyMapTravelSubsystem.h" 

UMyMapTravelStateComponent::UMyMapTravelStateComponent()
{
	// 传送组件纯粹基于状态驱动，不需要每帧 Tick，关闭以节省性能
	PrimaryComponentTick.bCanEverTick = false;

	// 默认状态为自由行动（非流送/非传送期间）
	DeploymentStatus = ETravelDeploymentStatus::Active;

	// 【核心基建】：组件网络同步的专属写法！
	// 必须开启此项，否则服务器的令牌变化绝对无法下发给客机的该组件！
	SetIsReplicatedByDefault(true);
}

void UMyMapTravelStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 注册核心流送令牌参与网络底层同步
	DOREPLIFETIME(UMyMapTravelStateComponent, DeploymentStatus);
}

void UMyMapTravelStateComponent::SetDeploymentStatus(ETravelDeploymentStatus NewStatus)
{
	// 【服务器独裁防线】（组件化适配）：
	// 1. 组件自身没有 HasAuthority，必须向上找它的宿主 (GetOwner，即 PlayerState) 来查验权限。
	// 2. 防抖过滤：如果状态没变，绝不触发无意义的广播。
	if (GetOwner() && GetOwner()->HasAuthority() && DeploymentStatus != NewStatus)
	{
		// 备份旧状态，供后续逻辑比对
		ETravelDeploymentStatus OldStatus = DeploymentStatus;

		// 权威更新令牌
		DeploymentStatus = NewStatus;

		// 【UE5 底层巨坑修复】：
		// 虚幻的网络机制规定：变量改变时，只会向“别人（客机）”触发 RepNotify。
		// 服务器自己本地的机器（也就是房主）是不会自动触发 OnRep_DeploymentStatus 的！
		// 所以必须在这里手动强行调用一次，确保“房主”和“副机”走的是同一套状态机响应管线。
		OnRep_DeploymentStatus(OldStatus);
	}
}

// 【客户端 Ack 握手 RPC】：
// 客户端本地 ScreenOff (熄屏/黑幕) UI 正式触发拉起后，主动向服务器发送确认包。
// 服务器接收后，将该玩家的状态从 Traveling 权威推进至 WaitingForShell (挂起等待物理搬运)。
void UMyMapTravelStateComponent::Server_AckScreenOffReady_Implementation()
{
	// 服务器收到 Ack，权威将状态推进至 WaitingForShell (黑幕已就绪，挂起等待物理躯壳排布)
	SetDeploymentStatus(ETravelDeploymentStatus::WaitingForShell);
}

void UMyMapTravelStateComponent::OnRep_DeploymentStatus(ETravelDeploymentStatus OldStatus)
{
	// 【客户端自动化闭环 1：表现层】
	// 广播给 UI 蓝图（如 MyLoadingScreenWidget），UI 接收到信号后自动判断是否擦除黑幕。
	OnDeploymentStatusChangedDelegate.Broadcast(DeploymentStatus);

	// 【客户端自动化闭环 2：物理与输入层】
	// 绝不在组件里写脏代码，直接把令牌和旧状态扔给“静默指挥官（子系统）”
	if (UWorld* World = GetWorld())
	{
		if (UMyMapTravelSubsystem* TravelSub = World->GetSubsystem<UMyMapTravelSubsystem>())
		{
			// 【架构解耦】：将 this (本组件) 传给导演，导演根据令牌的具体状态，
			// 对该玩家执行“剥夺控制权”、“拉起黑幕”或“恢复输入”
			TravelSub->HandleDeploymentTokenUpdate(this, OldStatus);
		}
	}
}
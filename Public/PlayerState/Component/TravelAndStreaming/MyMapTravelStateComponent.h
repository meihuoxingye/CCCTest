#pragma once

#include "CoreMinimal.h"
// 【核心修改】：从继承 PlayerState 改为继承最基础的 ActorComponent
#include "Components/ActorComponent.h"
#include "MyMapTravelStateComponent.generated.h"

// ==============================================================================
// 跨图与流送部署令牌 (Travel & Streaming Deployment Token)
// 核心契约：不仅锁死副机的UI和物理，更作为服务器执行业务逻辑的强制门禁！
// ==============================================================================
UENUM(BlueprintType)
enum class ETravelDeploymentStatus : uint8
{
	Active,           // 【自由行动】：跨图/流送完全结束，输入解封，服务器可安全执行后续业务逻辑 (默认状态)
	Traveling,        // 【跨界黑幕】：正在跨图或同图流送中，处于绝对黑幕期，彻底剥夺物理与UI控制权
	WaitingForShell,  // 【物理挂起】：逻辑已连入新世界，但肉体阵型/地板流送未就绪，主客机必须强制挂起
	ReadyToPossess,   // 【发牌解封】：服务器物理布阵完毕，通知客户端解封 UI 和输入，准备执行夺舍/附身
	Failed            // 【超时兜底】：流送超时或严重错误时的保护状态
};

// 声明多播委托，供本地的 UI 组件(加载屏)和底层输入严格订阅令牌变化
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTravelDeploymentStatusChanged, ETravelDeploymentStatus, NewStatus);

/**
 * 跨图与大世界流送专属的状态组件 (从原 PlayerState 降级而来)：
 * 专职负责统筹跨图、同图无缝流送时的“物理-网络-表现”时序安全。
 *
 * 【架构解耦规范】：
 * 本组件将被 C++ 底层死死焊在全局唯一的 AMyPlayerState 载体上。
 * 各个系统（如导演系统）只需通过 FindComponentByClass 索要本组件即可，彻底消除对特定 PlayerState 类的强依赖！
 */
UCLASS(ClassGroup = (MapTravel), meta = (BlueprintSpawnableComponent))
class CCC_API UMyMapTravelStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMyMapTravelStateComponent();

	// ==============================================================================
	// 核心状态令牌 (State Token)
	// ==============================================================================

	// 流送部署令牌：必须开启网络复制，并绑定 RepNotify 回调以自动化驱动客户端逻辑
	UPROPERTY(ReplicatedUsing = OnRep_DeploymentStatus, BlueprintReadOnly, Category = "MapTravel|Deployment")
	ETravelDeploymentStatus DeploymentStatus;

	// 供客户端 UI 和系统订阅的事件频道
	UPROPERTY(BlueprintAssignable, Category = "MapTravel|Deployment")
	FOnTravelDeploymentStatusChanged OnDeploymentStatusChangedDelegate;

	// 【服务器独裁接口】：只有服务器的大一统管线有资格拨动此令牌
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "MapTravel|Deployment")
	void SetDeploymentStatus(ETravelDeploymentStatus NewStatus);

	// 【客户端 Ack 握手 RPC】：
	// 客户端本地 ScreenOff (熄屏/黑幕) UI 正式触发拉起后，主动向服务器发送确认包。
	// 服务器接收后，将该玩家的状态从 Traveling 权威推进至 WaitingForShell (挂起等待物理搬运)。
	UFUNCTION(Server, Reliable)
	void Server_AckScreenOffReady();

protected:
	// 注册网络复制属性，虚幻联机基建
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 令牌翻转时的核心响应 (消灭一切副机越权操作的执行点)
	UFUNCTION()
	void OnRep_DeploymentStatus(ETravelDeploymentStatus OldStatus);
};
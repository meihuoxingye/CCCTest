// Fill out your copyright notice in the Description page of Project Settings.

#include "MyOnline/FaultHandling/NetworkErrorSubsystem.h"
// 引入你项目里真实的地图漫游管线头文件
#include "MapTravel/MyMapTravelSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"  // 【新增】：包含 ENetworkFailure 枚举定义
#include "Engine/GameInstance.h" // 【核心修复】：解决不完整类型，消灭满屏的 < > 语法报错！
#include "Engine/World.h"        // 【核心修复】：确保 UWorld 类型完整

void UNetworkErrorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 🛡️ 在子系统初始化时，直接绑定全局引擎网络失败委托
	if (GEngine)
	{
		FailureDelegateHandle = GEngine->OnNetworkFailure().AddUObject(this, &UNetworkErrorSubsystem::OnNetworkFailure);
	}
}

void UNetworkErrorSubsystem::Deinitialize()
{
	// 安全解绑委托，防止引擎在退出或重置时调用野指针
	if (GEngine && FailureDelegateHandle.IsValid())
	{
		GEngine->OnNetworkFailure().Remove(FailureDelegateHandle);
	}

	Super::Deinitialize();
}

void UNetworkErrorSubsystem::OnNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	// 记录错误，准备在表现层拦截引擎的默认行为
	UE_LOG(LogTemp, Error, TEXT("🛑 [NetworkErrorSubsystem] 网络断开，准备拦截 UI 表现层。错误详情: %s"), *ErrorString);

	// ==============================================================================
	// 【核心真相：为什么引擎非要“切换地图”？】
	// 1. 在虚幻引擎的底层架构中，UWorld（世界）与 UNetDriver（网络驱动）是强耦合的。
	// 2. 当加入联机房间时，当前的 UWorld 作为“从属世界”存在，其所有数据必须由网络驱动维持。
	// 3. 一旦网络断开，当前物理世界就变成了“被污染的孤岛”。
	// 4. 为了防止内存溢出、逻辑死循环或指针崩溃，UE 的底层硬性保护策略是：
	//    必须销毁当前世界，重新加载一个本地干净的世界。
	// ==============================================================================

	// ==============================================================================
	// 【保底切图拦截策略：原地进入当前世界的单机版】
	// 默认情况下，如果不干预，引擎会自动加载当前地图的单机副本（?closed），造成奇怪的突然切换感。
	// 我们无法阻止虚幻引擎的“切图”动作，因为那是它自我保护、清理内存的硬性手段。
	// 但现代联机游戏会利用“障眼法”，消灭掉这种物理切图的“可见性”。
	// 联机游戏的断连处理，本质上就是一场“用 UI 掩盖引擎重置过程”的魔术。
	// ==============================================================================

	// 1. 暴力解除可能残留的业务互斥锁
	// 【核心修复】：MyMapTravelSubsystem 是 UWorldSubsystem！必须用引擎传进来的 World 去拿！
	if (World)
	{
		if (UMyMapTravelSubsystem* TravelSub = World->GetSubsystem<UMyMapTravelSubsystem>())
		{
			// 强行恢复输入并解锁漫游状态，防止玩家带着黑幕/硬直状态进入底层的单机副本
			TravelSub->RestorePlayerInput();
		}
	}
	else
	{
		// 【新增防御】：防止极少数极度卡顿或预销毁阶段导致传进来的 World 指针为空
		UE_LOG(LogTemp, Warning, TEXT("⚠️ [NetworkErrorSubsystem] 断连时 World 为空，无法通过 WorldSubsystem 执行清理！"));
	}

	// 2. 核心拦截：利用“障眼法”隐藏底层的 ?closed 强制加载。
	// 真正的现代做法是原地冻结 + 弹窗，强行保留之前的 Loading 界面或黑幕，让玩家感觉自己还在原处。

	// ==============================================================================
	// TODO: 当前项目尚未实装报错或重连 UI，此处用注释替代未来的 UI 唤醒逻辑。
	// 未来需要在这里通知 UI 系统拉起一个优先级最高、全屏的“连接断开”模态窗口。
	// 
	// 伪代码示例：
	// if (UMyUIManagerSubsystem* UI = GetGameInstance()->GetSubsystem<UMyUIManagerSubsystem>())
	// {
	// 		// 弹出致命错误 UI，用来盖住引擎在背后偷偷进行的 ?closed 加载过程。
	// 		UI->ShowFatalNetworkError(ErrorString);
	// }
	// ==============================================================================

	// ==============================================================================
	// [✅ 已实装] 步骤 3：放行引擎底层自我保护机制 (Pass-through)
	// ==============================================================================
	// 【为什么这里除了 return 什么都没写？】因为“无为即是实装”。
	// 虚幻引擎底层在广播完 OnNetworkFailure 后，如果我们不强行调用 OpenLevel 打断管线，
	// 其 C++ 底层会自动执行 UEngine::Browse 去加载当前地图的单机副本 (?closed)。
	// 我们在这里显式调用 return 放行，就是完美利用引擎原生降级机制，完成废弃联机世界的安全销毁。
	// 
	// （注：因目前步骤 2 的全屏报错 UI 尚未接入，玩家依然会看到短暂黑屏并原地复活的突兀感。
	// 等未来 UI 接入后，这种底层的物理切图将被 UI 动画完美掩盖。）
	return;
}
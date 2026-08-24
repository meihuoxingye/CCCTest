// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Net/Core/Connection/NetEnums.h" // 【核心修复】：解决 ENetworkFailure 找不到的问题
#include "NetworkErrorSubsystem.generated.h"


// 【新增】：前向声明，告诉编译器这俩是指针类型，解决“找不到重载函数”的报错
class UWorld;
class UNetDriver;

/**
 * ==============================================================================
 * 网络异常监控与表现层兜底子系统 (Network Error Subsystem)
 *
 * 触发机制：
 * 继承自 UGameInstanceSubsystem，无需在 GameInstance 或任何配置中手动注册。
 * 虚幻引擎在创建大管家时会自动反射扫描、实例化本类，并伴随全局生命周期自动运行。
 * 
 * 职责：
 * 专门负责监控全服网络故障（如：丢包、超时、MissingLevelPackage 等踢出事件）。
 * 它从 GameInstance 中解耦，拦截引擎底层的强行断连切图（?closed），
 * 并负责通过 UI 表现层来掩盖底层物理世界的崩塌。
 * ==============================================================================
 */
UCLASS()
class CCC_API UNetworkErrorSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// 子系统生命周期钩子：在 GameInstance 创建时自动注册网络报错委托
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// 子系统生命周期钩子：在 GameInstance 销毁时安全注销委托，防止野指针
	virtual void Deinitialize() override;

private:
	// 核心拦截器函数：当虚幻引擎底层网络驱动 (NetDriver) 崩溃或主动断开连接时触发。
	// 
	// 功能与实装状态：
	// [✅ 已实装] 1. 强制解锁漫游管线 (MapTravelSubsystem) 可能遗留的黑屏与输入锁。
	// [🚧 未实装] 2. 通知 UI 系统拉起最高优先级的断线弹窗，遮挡画面（待 UI 系统实装后接入）。
	// [✅ 已实装] 3. 允许引擎在后台继续加载 ?closed 地图（未来配合弹窗实现画面冻结感）。
	// 
	// @param World 当前发生断连的物理世界
	// @param NetDriver 负责当前通信的网络驱动
	// @param FailureType 错误类型枚举（如 ConnectionLost, ConnectionTimeout 等）
	// @param ErrorString 具体的错误描述字符串
	void OnNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);

private:
	// 委托句柄，用于记录绑定的网络错误回调，以便在子系统销毁时安全解绑
	FDelegateHandle FailureDelegateHandle;
};
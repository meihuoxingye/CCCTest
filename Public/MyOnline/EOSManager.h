#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/TimerHandle.h"
// 引入必需的头文件
#include "Delegates/Delegate.h"
#include "EOSManager.generated.h"


// 声明一个动态多播委托，用于通知蓝图或 C++ 其他系统：OSSv2 账本已同步完毕
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnUserReadyForLobbyDelegate);


UCLASS()
class CCC_API UEOSManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()


    // ==============================================================================
    // 核心生命周期与组件 (Core Lifecycle & Components)
    // ==============================================================================
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;


    // ==============================================================================
    // 匿名登录逻辑 (Anonymous Login Logic)
    // ==============================================================================
public:
    void StartEOSLogin();


    // ==============================================================================
    // 诊断与调试 (Diagnostics & Debugging)
    // ==============================================================================
public:
    void DiagnoseEOSConfig();

    // 断线检测函数，由 C-API 提供底层状态读取
    void CheckConnectionStatus();

private:
    // 用于 5 秒循环检查的定时器句柄
    FTimerHandle ConnectionCheckTimer;


    // ==============================================================================
    // 凭据同步注册 (OSSv2 账本刷新)
    // ==============================================================================
public:
    // 暴露给蓝图的委托，静默同步成功后广播，建房逻辑可以绑定到这里
    UPROPERTY(BlueprintAssignable, Category = "EOS|Auth")
    FOnUserReadyForLobbyDelegate OnUserReadyForLobby;

    // 工业级静默同步函数：将 C-API 凭据同步至 OSSv2
    void SyncCAPIUserToOSSv2();
};
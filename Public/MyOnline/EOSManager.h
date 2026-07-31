#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/TimerHandle.h"
#include "EOSManager.generated.h"

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
};
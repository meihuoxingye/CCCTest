#pragma once

#include "CoreMinimal.h"
#include "Online/Auth.h"
#include "Engine/DeveloperSettings.h"
#include "EOSTestInjector.generated.h"


/* 
 * 联机测试注入器 (Multiplayer Test Injector)
 * 【机制说明：真实Epic号 vs 虚拟测试号】
 * 给后来的开发者：为了解决同一台电脑无法双开测试 P2P 联机的痛点，本项目在开发期采用了“双轨制”登录架构。
 *
 * 1. 真实Epic号 (Device ID 流程):
 *    - 触发条件：未开启测试注入，或玩家正式游玩时的标准流程。
 *    - 原理：引擎会抓取当前电脑的硬件特征（或 C-API 生成）作为“设备码”，向 Epic 申请一个匿名通行证。
 *    - 痛点：同一台电脑双开时，两个窗口会抓取到完全相同的设备特征，导致在 Epic 服务器端互相“顶号”（踢出房间），无法进行单机联机测试。
 *
 * 2. 虚拟测试号 (Custom ID 流程):
 *    - 触发条件：在项目设置中勾选 bEnableTestInjection，并通过 .bat 附加参数（如 -LocalUser=Alpha）启动。
 *    - 原理：拦截原生的登录请求，将认证类型强行篡改为 CustomId，并向 Epic 发送 -LocalUser 后面的字符串作为身份凭证。
 *    - 作用：完美欺骗 Epic 服务器，让它认为这两个窗口是来自两个不同地球角落的独立玩家，从而实现单机零冲突的双开建房与匹配测试。
 *    - 警告：此机制仅在 `#if !UE_BUILD_SHIPPING` (非正式打包) 模式下生效，正式发售包会自动剥离此功能。
 */


 // 声明专属日志类别
DECLARE_LOG_CATEGORY_EXTERN(LogEOSTest, Log, All);

// 定义全局日志宏：自带开关检测与原生变参支持
#define INJECTOR_LOG(Verbosity, Format, ...) \
{ \
    if (const UEOSTestInjectorSettings* Settings = GetDefault<UEOSTestInjectorSettings>()) \
    { \
        if (Settings->bEnableSuccessLog) \
        { \
            UE_LOG(LogEOSTest, Verbosity, Format, ##__VA_ARGS__); \
        } \
    } \
}


UCLASS(Config = Game, defaultconfig, meta = (DisplayName = "EOS联机单机双开测试"))
class UEOSTestInjectorSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UPROPERTY(Config, EditAnywhere, Category = "EOS Test")
    bool bEnableTestInjection = false;

    UPROPERTY(Config, EditAnywhere, Category = "EOS Test")
    bool bEnableSuccessLog = false;

    // 新增：每次登录前强制清除本地缓存的开关
    UPROPERTY(Config, EditAnywhere, Category = "EOS Test")
    bool bForceClearCacheBeforeLogin = false;
};

class FEOSTestInjector
{
    // ==============================================================================
    // 联机测试注入器 (Multiplayer Test Injector)
    // ==============================================================================
public:
    static bool TryInjectLoginParams(UE::Online::FAuthLogin::Params& OutParams);
    static void LogLoginSuccess(const FString& AccountId, bool bWasInjected);
    static void ClearEOSCacheIfNeeded();

    // 【新增】：集中接管来自 Manager 的所有系统报错、提示与心跳日志
    static void LogSystemMessage(const FString& Message, bool bIsError = false);
    static void LogHeartbeat(int32 NetworkStatus);
};
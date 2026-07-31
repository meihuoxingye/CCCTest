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


// 声明一个配置类，将其关联到 Game 的默认配置文件，并在虚幻引擎的项目设置面板中注册为 "EOS联机单机双开测试"
UCLASS(Config = Game, defaultconfig, meta = (DisplayName = "EOS联机单机双开测试"))
class UEOSTestInjectorSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    // 将该变量暴露给项目设置面板的 "EOS Test" 分类，并存入 Config 文件，用于全局控制是否启用本地发号器注入
    UPROPERTY(Config, EditAnywhere, Category = "EOS Test")
    bool bEnableTestInjection = false;

    // 将该变量暴露给项目设置面板，用于全局控制是否在屏幕和控制台输出注入器相关的成功日志及系统提示
    UPROPERTY(Config, EditAnywhere, Category = "EOS Test")
    bool bEnableSuccessLog = false;

    // 控制是否在执行登录逻辑前，自动抹除本地系统凭据里的 Epic 账号令牌和匿名设备码缓存
    UPROPERTY(Config, EditAnywhere, Category = "EOS Test")
    bool bForceClearCacheBeforeLogin = false;
};

// 声明测试注入器核心工具类，将所有的注入、拦截、诊断函数统一封装在此
class FEOSTestInjector
{
    // ==============================================================================
    // 联机测试注入器 (Multiplayer Test Injector)
    // ==============================================================================
public:
    // 尝试拦截并修改登录参数。内部会根据运行环境（编辑器或 Bat 传参）判断，如果条件符合则向 OutParams 注入发号器凭证，并返回 true
    static bool TryInjectLoginParams(UE::Online::FAuthLogin::Params& OutParams);

    // 登录成功后的统一日志分发函数，根据 bWasInjected 标记区分，输出不同的前缀（真实号/测试号）和屏幕颜色
    static void LogLoginSuccess(const FString& AccountId, bool bWasInjected);

    // 缓存清理入口函数，其内部会读取设置面板的清理开关，只有开启时才会真正调用底层 C-API 抹除本地凭据
    static void ClearEOSCacheIfNeeded();

    // 集中接管来自 Manager 的所有系统报错、提示与心跳日志
    // 作为兼容函数，处理那些未使用 INJECTOR_LOG 宏而是通过函数调用的系统级信息，同样受日志开关严格管控
    static void LogSystemMessage(const FString& Message, bool bIsError = false);

    // 专用的网络心跳监测日志函数，负责解析底层的 EOS_ENetworkStatus 枚举状态，并在屏幕上使用固定 Key 刷新绿字或红字警告
    static void LogHeartbeat(int32 NetworkStatus);
};
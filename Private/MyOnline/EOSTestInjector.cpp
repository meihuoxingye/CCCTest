#include "MyOnline/EOSTestInjector.h"
#include "Misc/CommandLine.h"
#include "Engine/Engine.h" 

// 引入原生 EOS C-API 与 UE5 共享模块用于清理缓存
#include "eos_platform_prereqs.h" 
#include "eos_sdk.h"         
#include "eos_connect.h"
#include "eos_auth.h"
#include "IEOSSDKManager.h"


// 定义专属日志类别
DEFINE_LOG_CATEGORY(LogEOSTest);


// ==============================================================================
// 联机测试注入器 (Multiplayer Test Injector)
// ==============================================================================
#pragma region

bool FEOSTestInjector::TryInjectLoginParams(UE::Online::FAuthLogin::Params& OutParams)
{
    // 获取测试注入器的全局默认配置对象，读取项目面板中的开关状态
    const UEOSTestInjectorSettings* Settings = GetDefault<UEOSTestInjectorSettings>();
    // 检查配置对象是否存在，以及测试注入开关是否被玩家开启
    if (!Settings || !Settings->bEnableTestInjection)
    {
        // 如果未开启，直接返回 false，让外部继续走原生的设备码登录流程
        return false;
    }

    // 预处理宏：确保注入器逻辑绝对不会被编译进正式发售的 Shipping 包中
#if !UE_BUILD_SHIPPING
    // 声明一个字符串，用于暂存解析出来的测试玩家身份标识（如 Alpha 或 Beta）
    FString TestLocalUser;

    // 拦截命令行中的 -LocalUser 参数
    // 检查启动游戏时的附加命令行参数，尝试提取 "LocalUser=" 后面的值存入 TestLocalUser
    if (FParse::Value(FCommandLine::Get(), TEXT("LocalUser="), TestLocalUser))
    {
        // 成功拿到 .bat 传来的 Alpha 或 Beta
    }
    // 智能判断：如果是在编辑器 (PIE) 里点运行，默认分配 Alpha 号！
    // 如果命令行里没有 -LocalUser，但当前是在虚幻编辑器内部运行（测试）
    else if (GIsEditor)
    {
        // 则自动给当前窗口分配 "Alpha" 作为身份标识，省去手动配参数的麻烦
        TestLocalUser = TEXT("Alpha");
    }
    // 如果既不是从包含参数的 bat 启动，也不是在编辑器里运行
    else
    {
        // 说明当前环境不满足双开注入条件，返回 false 退回标准流程
        return false;
    }

    // 强制将登录凭据类型篡改为 Developer（开发者工具模式），绕过默认的外部设备码模式
    OutParams.CredentialsType = UE::Online::LoginCredentialsType::Developer;
    // 将登录凭据的 ID 强制指向本地运行的 Epic 发号器工具 (Dev Auth Tool) 的地址和端口
    OutParams.CredentialsId = TEXT("127.0.0.1:8081");

    // 【核心修正1】：UE5 V2 明确规定 Developer 模式的 Token 必须是纯 FString！
    // 将提取到的测试身份标识（Alpha 或 Beta）作为 Token 放入凭据中
    OutParams.CredentialsToken.Emplace<FString>(TestLocalUser);

    // 【核心修正2】：Epic SDK 明确规定，发号器模拟真实账号登录必须要有基础权限，否则必报 invalid_params！
    // 申请必要的基础资料、在线状态和好友列表权限，以模拟最真实的玩家登录环境
    OutParams.Scopes = { TEXT("basic_profile"), TEXT("presence"), TEXT("friends_list") };

    // 注入成功，返回 true，通知外部已被接管
    return true;
#endif

    // 如果是在 Shipping 构建下，上述宏内的代码不会编译，直接返回 false
    return false;
}

void FEOSTestInjector::LogLoginSuccess(const FString& AccountId, bool bWasInjected)
{
    // 获取测试注入器的全局默认配置对象
    const UEOSTestInjectorSettings* Settings = GetDefault<UEOSTestInjectorSettings>();
    // 检查配置是否存在，以及成功日志输出开关是否被开启
    if (!Settings || !Settings->bEnableSuccessLog)
    {
        // 若未开启，直接返回，不产生任何日志性能开销
        return;
    }

    // 预处理宏：确保调试日志相关逻辑不会进入正式包
#if !UE_BUILD_SHIPPING
    // 根据传入的状态，自动判断并生成一目了然的前缀
    // 如果 bWasInjected 为 true，说明走的是发号器，否则说明走的是真实设备码
    FString LoginType = bWasInjected ? TEXT("【发号器测试号】(Developer)") : TEXT("【真实Epic号】(DeviceID)");

    // 打印一条明显的分割线，方便在海量日志中快速定位
    UE_LOG(LogTemp, Warning, TEXT("====================================================="));
    // 输出拼接好的登录类型和对应的账号 PUID 字符串
    UE_LOG(LogTemp, Warning, TEXT("%s 登录成功！ PUID: %s"), *LoginType, *AccountId);
    // 打印下半部分分割线
    UE_LOG(LogTemp, Warning, TEXT("====================================================="));

    // 检查全局引擎指针 GEngine 是否有效，用于在屏幕上渲染文字
    if (GEngine)
    {
        // 视觉区分：虚拟测试号为黄色警告，真实账号为安全绿色
        // 使用三元运算符根据注入状态选取不同的屏幕打印颜色
        FColor MsgColor = bWasInjected ? FColor::Yellow : FColor::Green;
        // 在屏幕左上角打印登录成功信息，Key 设为 -1 表示不覆盖旧消息，显示时长 30 秒
        GEngine->AddOnScreenDebugMessage(-1, 30.0f, MsgColor, FString::Printf(TEXT("%s 登录成功 PUID: %s"), *LoginType, *AccountId));
    }
#endif
}

void FEOSTestInjector::ClearEOSCacheIfNeeded()
{
    // 获取测试注入器的配置对象
    const UEOSTestInjectorSettings* Settings = GetDefault<UEOSTestInjectorSettings>();
    // 检查强制清除缓存的开关是否被开启，如果没开则直接退出，保证无用开销为零
    if (!Settings || !Settings->bForceClearCacheBeforeLogin)
    {
        return;
    }

    // 获取 EOS SDK 管理器的单例，准备调用底层 C-API
    IEOSSDKManager* SDKManager = IEOSSDKManager::Get();
    // 如果获取失败则安全返回
    if (!SDKManager) return;

    // 提取当前活跃的所有 EOS 平台句柄
    TArray<IEOSPlatformHandlePtr> ActivePlatforms = SDKManager->GetActivePlatforms();
    // 确认获取到的平台句柄数组有效且非空
    if (ActivePlatforms.Num() == 0 || !ActivePlatforms[0].IsValid()) return;

    // 解引用获取底层的原生 C-API 平台句柄
    EOS_HPlatform NativePlatform = *ActivePlatforms[0];

    // 1. 抹除 Epic 账号的本地令牌缓存
    // 从原生平台句柄中获取 Auth 接口
    EOS_HAuth AuthHandle = EOS_Platform_GetAuthInterface(NativePlatform);
    // 如果成功拿到 Auth 接口句柄
    if (AuthHandle)
    {
        // 初始化删除持久化授权凭据的配置选项
        EOS_Auth_DeletePersistentAuthOptions DeleteAuthOpts = { 0 };
        // 设定 API 版本为对应的最新版本
        DeleteAuthOpts.ApiVersion = EOS_AUTH_DELETEPERSISTENTAUTH_API_LATEST;

        // 呼叫 C-API 异步删除本地存储的 Epic 账号登录令牌缓存
        EOS_Auth_DeletePersistentAuth(AuthHandle, &DeleteAuthOpts, nullptr, [](const EOS_Auth_DeletePersistentAuthCallbackInfo* Data)
            {
                // 判断删除操作是否成功返回
                if (Data->ResultCode == EOS_EResult::EOS_Success)
                {
                    // 打印账号令牌缓存已被抹除的提示日志
                    UE_LOG(LogTemp, Warning, TEXT("[缓存清理] Epic Persistent Auth (账号令牌) 已彻底从系统凭据中抹除！"));
                }
            });
    }

    // 2. 抹除 本地匿名设备码缓存
    // 从原生平台句柄中获取 Connect (连接服务) 接口
    EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(NativePlatform);
    // 如果成功拿到 Connect 接口句柄
    if (ConnectHandle)
    {
        // 初始化删除设备 ID 的配置选项
        EOS_Connect_DeleteDeviceIdOptions DeleteDeviceOpts = { 0 };
        // 设定 API 版本为对应的最新版本
        DeleteDeviceOpts.ApiVersion = EOS_CONNECT_DELETEDEVICEID_API_LATEST;

        // 呼叫 C-API 异步删除存储在本地系统里的匿名设备码
        EOS_Connect_DeleteDeviceId(ConnectHandle, &DeleteDeviceOpts, nullptr, [](const EOS_Connect_DeleteDeviceIdCallbackInfo* Data)
            {
                // 判断删除操作是否成功返回
                if (Data->ResultCode == EOS_EResult::EOS_Success)
                {
                    // 打印本地设备码缓存已被抹除的提示日志
                    UE_LOG(LogTemp, Warning, TEXT("[缓存清理] Device ID (本地设备码) 已彻底从系统凭据中抹除！"));
                }
            });
    }
}

void FEOSTestInjector::LogSystemMessage(const FString& Message, bool bIsError)
{
    // 获取测试注入器的配置对象
    const UEOSTestInjectorSettings* Settings = GetDefault<UEOSTestInjectorSettings>();
    // 统一受控于日志开关，一旦关闭，不产生任何性能开销
    if (!Settings || !Settings->bEnableSuccessLog)
    {
        // 未开启日志开关，直接拦截抛弃信息
        return;
    }

    // 预处理宏：限制仅在非正式发布环境下编译
#if !UE_BUILD_SHIPPING
    // 检查传入的参数是否被标记为错误类型
    if (bIsError)
    {
        // 强行使用 LogTemp 的 Error 级别打印红色的错误日志
        UE_LOG(LogTemp, Error, TEXT("%s"), *Message);
    }
    else
    {
        // 否则作为 Warning 级别打印黄色的提示日志
        UE_LOG(LogTemp, Warning, TEXT("%s"), *Message);
    }
#endif
}

void FEOSTestInjector::LogHeartbeat(int32 NetworkStatus)
{
    // 获取测试注入器的配置对象
    const UEOSTestInjectorSettings* Settings = GetDefault<UEOSTestInjectorSettings>();
    // 检查成功日志开关是否开启，心跳同样受此开关全局控制
    if (!Settings || !Settings->bEnableSuccessLog)
    {
        // 如果未开启则直接返回，保证心跳检测本身零性能消耗
        return;
    }

    // 预处理宏：防止调试心跳污染正式版
#if !UE_BUILD_SHIPPING
    // 检查全局引擎指针 GEngine 是否有效
    if (GEngine)
    {
        // 【核心修复】：2 才是真正的 EOS_NS_Online (在线)！
        // 校验底层返回的网络状态枚举值是否等于 2 (在线)
        if (NetworkStatus == 2)
        {
            // 在屏幕上打印绿色的稳定在线提示。使用固定的 Key (2026)，确保每次都在同一个位置刷新，不会刷屏
            GEngine->AddOnScreenDebugMessage(2026, 3.5f, FColor::Green, TEXT("[心跳监测] EOS 联机状态: 稳定在线 (Online)"));
        }
        else
        {
            // 如果状态不等于 2，说明已经离线或断开，在屏幕上打印红色的严重警告
            GEngine->AddOnScreenDebugMessage(2026, 3.5f, FColor::Red, TEXT("[心跳监测] 严重警告：EOS 连接已断开！(Offline)"));
            // 同时将断联情况及具体的枚举错误码写入输出日志中，使用专属的 LogEOSTest 类别
            UE_LOG(LogEOSTest, Error, TEXT("检测到 EOS 断联！当前状态枚举值: %d"), NetworkStatus);
        }
    }
#endif
}

#pragma endregion
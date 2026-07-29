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
    const UEOSTestInjectorSettings* Settings = GetDefault<UEOSTestInjectorSettings>();
    if (!Settings || !Settings->bEnableTestInjection)
    {
        return false;
    }

#if !UE_BUILD_SHIPPING
    FString TestLocalUser;

    // 拦截命令行中的 -LocalUser 参数
    if (FParse::Value(FCommandLine::Get(), TEXT("LocalUser="), TestLocalUser))
    {
        // 成功拿到 .bat 传来的 Alpha 或 Beta
    }
    // 智能判断：如果是在编辑器 (PIE) 里点运行，默认分配 Alpha 号！
    else if (GIsEditor)
    {
        TestLocalUser = TEXT("Alpha");
    }
    else
    {
        return false;
    }

    OutParams.CredentialsType = UE::Online::LoginCredentialsType::Developer;
    OutParams.CredentialsId = TEXT("127.0.0.1:8081");

    // 【核心修正1】：UE5 V2 明确规定 Developer 模式的 Token 必须是纯 FString！
    OutParams.CredentialsToken.Emplace<FString>(TestLocalUser);

    // 【核心修正2】：Epic SDK 明确规定，发号器模拟真实账号登录必须要有基础权限，否则必报 invalid_params！
    OutParams.Scopes = { TEXT("basic_profile"), TEXT("presence"), TEXT("friends_list") };

    return true;
#endif

    return false;
}

void FEOSTestInjector::LogLoginSuccess(const FString& AccountId, bool bWasInjected)
{
    const UEOSTestInjectorSettings* Settings = GetDefault<UEOSTestInjectorSettings>();
    if (!Settings || !Settings->bEnableSuccessLog)
    {
        return;
    }

#if !UE_BUILD_SHIPPING
    // 根据传入的状态，自动判断并生成一目了然的前缀
    FString LoginType = bWasInjected ? TEXT("【发号器测试号】(Developer)") : TEXT("【真实Epic号】(DeviceID)");

    UE_LOG(LogTemp, Warning, TEXT("====================================================="));
    UE_LOG(LogTemp, Warning, TEXT("%s 登录成功！ PUID: %s"), *LoginType, *AccountId);
    UE_LOG(LogTemp, Warning, TEXT("====================================================="));

    if (GEngine)
    {
        // 视觉区分：虚拟测试号为黄色警告，真实账号为安全绿色
        FColor MsgColor = bWasInjected ? FColor::Yellow : FColor::Green;
        GEngine->AddOnScreenDebugMessage(-1, 30.0f, MsgColor, FString::Printf(TEXT("%s 登录成功 PUID: %s"), *LoginType, *AccountId));
    }
#endif
}

// 新增：判断并执行缓存清理
void FEOSTestInjector::ClearEOSCacheIfNeeded()
{
    const UEOSTestInjectorSettings* Settings = GetDefault<UEOSTestInjectorSettings>();
    if (!Settings || !Settings->bForceClearCacheBeforeLogin)
    {
        return;
    }

    IEOSSDKManager* SDKManager = IEOSSDKManager::Get();
    if (!SDKManager) return;

    TArray<IEOSPlatformHandlePtr> ActivePlatforms = SDKManager->GetActivePlatforms();
    if (ActivePlatforms.Num() == 0 || !ActivePlatforms[0].IsValid()) return;

    EOS_HPlatform NativePlatform = *ActivePlatforms[0];

    // 1. 抹除 Epic 账号的本地令牌缓存
    EOS_HAuth AuthHandle = EOS_Platform_GetAuthInterface(NativePlatform);
    if (AuthHandle)
    {
        EOS_Auth_DeletePersistentAuthOptions DeleteAuthOpts = { 0 };
        DeleteAuthOpts.ApiVersion = EOS_AUTH_DELETEPERSISTENTAUTH_API_LATEST;

        EOS_Auth_DeletePersistentAuth(AuthHandle, &DeleteAuthOpts, nullptr, [](const EOS_Auth_DeletePersistentAuthCallbackInfo* Data)
            {
                if (Data->ResultCode == EOS_EResult::EOS_Success)
                {
                    UE_LOG(LogTemp, Warning, TEXT("[缓存清理] Epic Persistent Auth (账号令牌) 已彻底从系统凭据中抹除！"));
                }
            });
    }

    // 2. 抹除 本地匿名设备码缓存
    EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(NativePlatform);
    if (ConnectHandle)
    {
        EOS_Connect_DeleteDeviceIdOptions DeleteDeviceOpts = { 0 };
        DeleteDeviceOpts.ApiVersion = EOS_CONNECT_DELETEDEVICEID_API_LATEST;

        EOS_Connect_DeleteDeviceId(ConnectHandle, &DeleteDeviceOpts, nullptr, [](const EOS_Connect_DeleteDeviceIdCallbackInfo* Data)
            {
                if (Data->ResultCode == EOS_EResult::EOS_Success)
                {
                    UE_LOG(LogTemp, Warning, TEXT("[缓存清理] Device ID (本地设备码) 已彻底从系统凭据中抹除！"));
                }
            });
    }
}

void FEOSTestInjector::LogSystemMessage(const FString& Message, bool bIsError)
{
    const UEOSTestInjectorSettings* Settings = GetDefault<UEOSTestInjectorSettings>();
    // 统一受控于日志开关，一旦关闭，不产生任何性能开销
    if (!Settings || !Settings->bEnableSuccessLog)
    {
        return;
    }

#if !UE_BUILD_SHIPPING
    if (bIsError)
    {
        UE_LOG(LogTemp, Error, TEXT("%s"), *Message);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("%s"), *Message);
    }
#endif
}

void FEOSTestInjector::LogHeartbeat(int32 NetworkStatus)
{
    const UEOSTestInjectorSettings* Settings = GetDefault<UEOSTestInjectorSettings>();
    if (!Settings || !Settings->bEnableSuccessLog)
    {
        return;
    }

#if !UE_BUILD_SHIPPING
    if (GEngine)
    {
        // 【核心修复】：2 才是真正的 EOS_NS_Online (在线)！
        if (NetworkStatus == 2)
        {
            GEngine->AddOnScreenDebugMessage(2026, 3.5f, FColor::Green, TEXT("[心跳监测] EOS 联机状态: 稳定在线 (Online)"));
        }
        else
        {
            GEngine->AddOnScreenDebugMessage(2026, 3.5f, FColor::Red, TEXT("[心跳监测] 严重警告：EOS 连接已断开！(Offline)"));
            UE_LOG(LogEOSTest, Error, TEXT("检测到 EOS 断联！当前状态枚举值: %d"), NetworkStatus);
        }
    }
#endif
}

#pragma endregion
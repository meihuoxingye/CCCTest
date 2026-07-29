#include "MyOnline/EOSManager.h"
#include "MyOnline/EOSTestInjector.h" 
#include "Engine/GameInstance.h"
#include "Engine/TimerHandle.h"
#include "TimerManager.h"
#include "Online/Auth.h"
#include "Online/OnlineServices.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "Online/OnlineAsyncOpHandle.h"
#include "Online/OnlineResult.h"
#include "Online/OnlineError.h"

// ==============================================================================
// 引入原生 EOS C-API 与 UE5 共享模块
// ==============================================================================
#include "eos_platform_prereqs.h" 
#include "eos_sdk.h"         
#include "eos_connect.h"
#include "eos_auth.h"
#include "IEOSSDKManager.h"

#include "Misc/ConfigCacheIni.h" // 顶部确保有这个头文件用于动态修改配置

// ==============================================================================
// 核心生命周期与组件 (Core Lifecycle & Components)
// ==============================================================================
#pragma region

void UEOSManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    FTimerHandle TimerHandle;
    GetGameInstance()->GetTimerManager().SetTimer(TimerHandle, this, &UEOSManager::StartEOSLogin, 1.0f, false);
}

#pragma endregion

// ==============================================================================
// 匿名登录逻辑 (Anonymous Login Logic)
// ==============================================================================
#pragma region
void UEOSManager::StartEOSLogin()
{
    // 【仅新增此行】：在执行任何登录流程前，检查并清理底层缓存
    FEOSTestInjector::ClearEOSCacheIfNeeded();

    using namespace UE::Online;

    FAuthLogin::Params Params{};

    // 【核心修复：无论真假号，都必须告诉引擎是本地的哪号玩家在操作！】
    Params.PlatformUserId = IPlatformInputDeviceMapper::Get().GetPrimaryPlatformUser();

    // 核心修改：用一个变量记住这次有没有被文件接管
    bool bIsInjected = FEOSTestInjector::TryInjectLoginParams(Params);

    // ==============================================================================
        // 【核心修复：强制唤醒引擎 V2 模块】
        // 为什么要放在这里而不是后面？
        // 因为本类继承自 UGameInstanceSubsystem，在 Standalone/Bat 独立模式下启动极早（冷启动）。
        // 此时虚幻引擎的 EOS 模块出于节省性能的考虑，正处于“懒加载（未初始化）”状态。
        // 如果不主动调用一次 GetServices，底层的 ActivePlatforms 将会为空，导致后续 C-API 静默返回，代码彻底死亡。
        // 在这里提前调用 GetServices，相当于一个“唤醒信号”，逼迫引擎立刻把懒加载的 EOS Platform 句柄实例化出来，
        // 从而保证下面的 C-API 越狱通道能够顺利拿到活跃的底层句柄。
        //
        // 【避坑警告：千万不要自创 InstanceName】
        // 必须直接使用 EOnlineServices::Default！如果在 Bat 模式下传自定义名字（如 DevInstance），
        // 引擎会因为找不到对应的 INI 配置，强行塞入一个废弃的测试假 ClientId (xyza789...)，
        // 最终导致发号器被 Epic 服务器报 1012 (无效客户端) 错误拒绝登录！
        // ==============================================================================
    IOnlineServicesPtr OnlineServices = GetServices(EOnlineServices::Default);

    if (!OnlineServices.IsValid())
    {
        INJECTOR_LOG(Error, TEXT("严重错误：GetServices 未能有效唤醒引擎 OnlineServices！"));
        return;
    }

    // ==============================================================================
    // 注入器越狱通道 (Bypass V2 Architecture)
    // 绕过 V2 底层不支持在 EASAuthEnabled=false 时使用 Developer 枚举的缺陷，直接呼叫 C-API
    // ==============================================================================
    if (bIsInjected)
    {
        IEOSSDKManager* SDKManager = IEOSSDKManager::Get();
        if (!SDKManager) return;

        TArray<IEOSPlatformHandlePtr> ActivePlatforms = SDKManager->GetActivePlatforms();
        if (ActivePlatforms.Num() == 0 || !ActivePlatforms[0].IsValid())
        {
            INJECTOR_LOG(Error, TEXT("严重错误：虽然调用了 GetServices，但底层 EOS Platform 仍未激活！"));
            return;
        }

        EOS_HPlatform NativePlatform = *ActivePlatforms[0];
        EOS_HAuth AuthHandle = EOS_Platform_GetAuthInterface(NativePlatform);
        if (!AuthHandle) return;

        // 必须在作用域内保持 UTF8 字符串存活
        FString HostPort = Params.CredentialsId;
        FString TokenStr = Params.CredentialsToken.Get<FString>();
        FTCHARToUTF8 Utf8Id(*HostPort);
        FTCHARToUTF8 Utf8Token(*TokenStr);

        EOS_Auth_Credentials Credentials = { 0 };
        Credentials.ApiVersion = EOS_AUTH_CREDENTIALS_API_LATEST;
        Credentials.Type = EOS_ELoginCredentialType::EOS_LCT_Developer;
        Credentials.Id = Utf8Id.Get();
        Credentials.Token = Utf8Token.Get();

        // 第一次注入器登录时，将上面三行临时替换为下面代码，第一次成功后还原为上面三行
        // Credentials.Type = EOS_ELoginCredentialType::EOS_LCT_AccountPortal;
        // Credentials.Id = nullptr;
        // Credentials.Token = nullptr;

        EOS_Auth_LoginOptions LoginOptions = { 0 };
        LoginOptions.ApiVersion = EOS_AUTH_LOGIN_API_LATEST;
        LoginOptions.Credentials = &Credentials;

        LoginOptions.ScopeFlags = (EOS_EAuthScopeFlags)(
            EOS_EAuthScopeFlags::EOS_AS_BasicProfile |
            EOS_EAuthScopeFlags::EOS_AS_FriendsList |
            EOS_EAuthScopeFlags::EOS_AS_Presence |
            EOS_EAuthScopeFlags::EOS_AS_Country
            );

        // 【加上这行】：在发起异步请求前，明确打印开始行动
        INJECTOR_LOG(Warning, TEXT("正在向 Epic 服务器发起 [开发者测试号] 登录请求，等待回应中..."));

        EOS_Auth_Login(AuthHandle, &LoginOptions, this, [](const EOS_Auth_LoginCallbackInfo* Data)
            {
                if (Data->ResultCode == EOS_EResult::EOS_Success)
                {
                    INJECTOR_LOG(Warning, TEXT(">> C-API 底层发号器模拟登录成功！ <<"));

                    char AccountIdString[EOS_EPICACCOUNTID_MAX_LENGTH + 1];
                    int32_t BufferSize = sizeof(AccountIdString);
                    if (EOS_EpicAccountId_ToString(Data->LocalUserId, AccountIdString, &BufferSize) == EOS_EResult::EOS_Success)
                    {
                        FString PUIDString = UTF8_TO_TCHAR(AccountIdString);
                        // 把真假状态明确地传给日志
                        INJECTOR_LOG(Warning, TEXT("【发号器测试号】(Developer) 登录成功！ PUID: %s"), *PUIDString);
                    }

                    // 【新增】：登录成功后，开启每 5 秒一次的断线监测循环
                    UEOSManager* Manager = static_cast<UEOSManager*>(Data->ClientData);
                    if (Manager && Manager->GetGameInstance())
                    {
                        Manager->GetGameInstance()->GetTimerManager().SetTimer(Manager->ConnectionCheckTimer, Manager, &UEOSManager::CheckConnectionStatus, 5.0f, true);
                    }
                }
                else
                {
                    INJECTOR_LOG(Error, TEXT("C-API 发号器登录失败！错误码: %d (EOS_Result: %s)"),
                        (int32)Data->ResultCode,
                        UTF8_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
                }
            });

        // 拦截完成，直接退出，绝不让 V2 碰我们的发号器参数！
        return;
    }

    // ==============================================================================
    // 正常匿名通道 (V2 DeviceID Logic)
    // ==============================================================================
    IAuthPtr AuthInterface = OnlineServices->GetAuthInterface();
    if (!AuthInterface.IsValid()) return;

    Params.CredentialsType = LoginCredentialsType::ExternalAuth;

    FExternalAuthToken DeviceToken;
    DeviceToken.Type = ExternalLoginType::DeviceIdAccessToken;
    DeviceToken.Data = FString();

    Params.CredentialsToken.Emplace<FExternalAuthToken>(MoveTemp(DeviceToken));

    // 只有在走真实设备码账号时，才清空 CredentialsId
    Params.CredentialsId = FString();

    // 真实匿名设备码登录不需要任何权限
    Params.Scopes = {};

    // 【加上这行】：在发起异步请求前，明确打印开始行动
    INJECTOR_LOG(Warning, TEXT("正在向 Epic 服务器发起 [匿名设备码] 登录请求，等待回应中..."));

    // 核心修改：把 bIsInjected 传进回调里
    AuthInterface->Login(MoveTemp(Params))
        .OnComplete(this, [this](const TOnlineResult<FAuthLogin>& Result)
            {
                if (Result.IsOk())
                {
                    const TSharedRef<FAccountInfo> AccountInfo = Result.GetOkValue().AccountInfo;
                    FString PUIDString = ToLogString(AccountInfo->AccountId);

                    // 把真假状态明确地传给日志
                    INJECTOR_LOG(Warning, TEXT("【真实Epic号】(DeviceID) 登录成功！ PUID: %s"), *PUIDString);

                    // 【新增】：登录成功后，开启每 5 秒一次的断线监测循环
                    if (GetGameInstance())
                    {
                        GetGameInstance()->GetTimerManager().SetTimer(ConnectionCheckTimer, this, &UEOSManager::CheckConnectionStatus, 5.0f, true);
                    }
                }
                else
                {
                    const FOnlineError& Error = Result.GetErrorValue();
                    FString ErrorId = Error.GetErrorId();

                    if (Error.GetLogString().Contains(TEXT("not_found")) || Error.GetLogString().Contains(TEXT("EOS_NotFound")) || ErrorId.Contains(TEXT("1.1.12")))
                    {
                        INJECTOR_LOG(Warning, TEXT("[拦截成功] V2 报告本地无设备码，正在触发 C-API 底层生成..."));

                        IEOSSDKManager* SDKManager = IEOSSDKManager::Get();
                        if (!SDKManager) return;

                        TArray<IEOSPlatformHandlePtr> ActivePlatforms = SDKManager->GetActivePlatforms();
                        if (ActivePlatforms.Num() == 0 || !ActivePlatforms[0].IsValid()) return;

                        EOS_HPlatform NativePlatform = *ActivePlatforms[0];
                        EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(NativePlatform);
                        if (!ConnectHandle) return;

                        EOS_Connect_CreateDeviceIdOptions CreateOptions = { 0 };
                        CreateOptions.ApiVersion = EOS_CONNECT_CREATEDEVICEID_API_LATEST;
                        CreateOptions.DeviceModel = "PC";

                        EOS_Connect_CreateDeviceId(ConnectHandle, &CreateOptions, this, [](const EOS_Connect_CreateDeviceIdCallbackInfo* Data)
                            {
                                if (Data->ResultCode == EOS_EResult::EOS_Success)
                                {
                                    INJECTOR_LOG(Warning, TEXT(">> C-API 底层静默生成成功！正在重新唤起 V2 登录... <<"));

                                    UEOSManager* Manager = static_cast<UEOSManager*>(Data->ClientData);
                                    if (Manager)
                                    {
                                        Manager->StartEOSLogin();
                                    }
                                }
                                else
                                {
                                    INJECTOR_LOG(Error, TEXT("C-API 生成设备码失败！错误码: %d"), (int32)Data->ResultCode);
                                }
                            });
                    }
                    else
                    {
                        INJECTOR_LOG(Error, TEXT("V2 登录发生其他错误: %s"), *Error.GetLogString());
                    }
                }
            });
}
#pragma endregion

// ==============================================================================
// 诊断与调试 (Diagnostics & Debugging)
// ==============================================================================
#pragma region

void UEOSManager::DiagnoseEOSConfig()
{
    INJECTOR_LOG(Log, TEXT("诊断模块就绪。"));
}

void UEOSManager::CheckConnectionStatus()
{
    IEOSSDKManager* SDKManager = IEOSSDKManager::Get();
    if (!SDKManager) return;

    TArray<IEOSPlatformHandlePtr> ActivePlatforms = SDKManager->GetActivePlatforms();
    if (ActivePlatforms.Num() == 0 || !ActivePlatforms[0].IsValid()) return;

    EOS_HPlatform NativePlatform = *ActivePlatforms[0];
    EOS_ENetworkStatus NetworkStatus = EOS_Platform_GetNetworkStatus(NativePlatform);

    FEOSTestInjector::LogHeartbeat((int32)NetworkStatus);
}

#pragma endregion
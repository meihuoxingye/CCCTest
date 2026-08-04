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

#include "Misc/ScopeExit.h" // 【新增】：用于 ON_SCOPE_EXIT 宏，确保 C-API 句柄绝对释放
#include "Engine/Engine.h"  // 【新增】：用于未来断线时调用 GEngine->HandleDisconnect

// ==============================================================================
// 核心生命周期与组件 (Core Lifecycle & Components)
// ==============================================================================
#pragma region

void UEOSManager::Initialize(FSubsystemCollectionBase& Collection)
{
    // 调用父类 UGameInstanceSubsystem 的初始化逻辑，确保基础系统正常启动
    Super::Initialize(Collection);

    // 声明一个定时器句柄，用于控制延时执行的生命周期
    FTimerHandle TimerHandle;
    // 在 GameInstance 的定时器管理器中设置一个 1.0 秒后触发的单次定时器，用来延时调用 StartEOSLogin，避免引擎尚未完全加载导致的崩溃
    GetGameInstance()->GetTimerManager().SetTimer(TimerHandle, this, &UEOSManager::StartEOSLogin, 1.0f, false);
}

#pragma endregion

// ==============================================================================
// 匿名登录逻辑 (Anonymous Login Logic)
// ==============================================================================
#pragma region

// 【补丁 B 核心】：专门用于 C-API 跨域异步传输的安全上下文结构体
// 将原本分配在栈 (Stack) 上的临时字符串，强行装入堆 (Heap) 中，
// 确保在 EOS_Auth_Login 异步回调返回前，字符串内存绝对不会被系统回收！
struct FEOSLoginContext
{
    UEOSManager* Manager;
    FTCHARToUTF8* Utf8Id;
    FTCHARToUTF8* Utf8Token;

    FEOSLoginContext(UEOSManager* InManager, const FString& InId, const FString& InToken)
        : Manager(InManager)
    {
        Utf8Id = new FTCHARToUTF8(*InId);
        Utf8Token = new FTCHARToUTF8(*InToken);
    }

    ~FEOSLoginContext()
    {
        delete Utf8Id;
        delete Utf8Token;
    }
};

void UEOSManager::StartEOSLogin()
{
    // 在执行任何登录流程前，检查并清理底层缓存
    // 函数内部会读取 bForceClearCacheBeforeLogin 开关，若未开启则直接 return，无任何性能开销
    FEOSTestInjector::ClearEOSCacheIfNeeded();

    // 引入虚幻引擎 Online 命名空间，方便后续使用相关的结构体和枚举
    using namespace UE::Online;

    // 实例化一个登录参数结构体，用于配置后续传给 V2 在线子系统的各项登录凭据
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

    // 检查获取到的 OnlineServices 指针是否有效，若无效说明引擎在线服务加载彻底失败
    if (!OnlineServices.IsValid())
    {
        // 通过注入器宏输出严重错误日志，提示获取引擎在线服务失败
        INJECTOR_LOG(Error, TEXT("严重错误：GetServices 未能有效唤醒引擎 OnlineServices！"));
        // 终止后续所有的登录尝试，安全退出
        return;
    }

    // ==============================================================================
    // 注入器越狱通道 (Bypass V2 Architecture)
    // 绕过 V2 底层不支持在 EASAuthEnabled=false 时使用 Developer 枚举的缺陷，直接呼叫 C-API
    // ==============================================================================
    if (bIsInjected)
    {
        // 获取 EOS SDK 管理器的单例指针，用来访问底层 API
        IEOSSDKManager* SDKManager = IEOSSDKManager::Get();
        // 如果管理器指针为空，直接返回防止崩溃
        if (!SDKManager) return;

        // 从管理器中获取当前所有已激活的 EOS 平台句柄数组
        TArray<IEOSPlatformHandlePtr> ActivePlatforms = SDKManager->GetActivePlatforms();
        // 检查平台数组是否为空，或者第一个句柄是否无效
        if (ActivePlatforms.Num() == 0 || !ActivePlatforms[0].IsValid())
        {
            // 如果没有拿到有效句柄，说明底层的 EOS 平台实例并未激活，打印错误
            INJECTOR_LOG(Error, TEXT("严重错误：虽然调用了 GetServices，但底层 EOS Platform 仍未激活！"));
            // 终止登录逻辑并退出
            return;
        }

        // 解引用获取底层的原生 C-API 平台句柄 (EOS_HPlatform)
        EOS_HPlatform NativePlatform = *ActivePlatforms[0];
        // 从原生平台句柄中请求获取 Auth (身份验证) 接口句柄
        EOS_HAuth AuthHandle = EOS_Platform_GetAuthInterface(NativePlatform);
        // 如果没拿到 Auth 接口句柄，直接退出
        if (!AuthHandle) return;

        // 必须在作用域内保持 UTF8 字符串存活
        FString HostPort = Params.CredentialsId;
        // 将传递过来的登录凭证 Token 提取为字符串
        FString TokenStr = Params.CredentialsToken.Get<FString>();

        // 【补丁 B 应用】：实例化堆内存上下文，接管字符串生命周期！
        // 取代了之前直接在栈上 FTCHARToUTF8 的操作
        FEOSLoginContext* LoginContext = new FEOSLoginContext(this, HostPort, TokenStr);

        // 初始化 EOS C-API 的身份验证凭据结构体，全部置 0 清除内存脏数据
        EOS_Auth_Credentials Credentials = { 0 };
        // 设置当前使用的 API 版本，确保与 SDK 头文件匹配
        Credentials.ApiVersion = EOS_AUTH_CREDENTIALS_API_LATEST;
        // 指定使用开发者工具 (Dev Auth Tool) 凭据类型进行模拟登录
        Credentials.Type = EOS_ELoginCredentialType::EOS_LCT_Developer;
        // 传入本地发号器的 IP 和端口 (直接从堆内存安全截取 C 风格指针)
        Credentials.Id = LoginContext->Utf8Id->Get();
        // 传入在发号器中创建的测试凭据名称 (直接从堆内存安全截取 C 风格指针)
        Credentials.Token = LoginContext->Utf8Token->Get();

        // 第一次注入器登录时，将上面三行临时替换为下面代码，第一次成功后还原为上面三行
        // Credentials.Type = EOS_ELoginCredentialType::EOS_LCT_AccountPortal;
        // Credentials.Id = nullptr;
        // Credentials.Token = nullptr;

        // 初始化 EOS C-API 的登录选项结构体，并全部置 0
        EOS_Auth_LoginOptions LoginOptions = { 0 };
        // 设置当前使用的登录 API 版本
        LoginOptions.ApiVersion = EOS_AUTH_LOGIN_API_LATEST;
        // 绑定上面配置好的账户凭据结构体指针
        LoginOptions.Credentials = &Credentials;

        // 按位或配置申请的权限范围 (ScopeFlags)，赋予当前用户需要的基础功能权限：基础个人资料、好友列表、在线状态、国家/地区信息
        LoginOptions.ScopeFlags = (EOS_EAuthScopeFlags)(
            EOS_EAuthScopeFlags::EOS_AS_BasicProfile |
            EOS_EAuthScopeFlags::EOS_AS_FriendsList |
            EOS_EAuthScopeFlags::EOS_AS_Presence |
            EOS_EAuthScopeFlags::EOS_AS_Country
            );

        // 【加上这行】：在发起异步请求前，明确打印开始行动
        INJECTOR_LOG(Warning, TEXT("正在向 Epic 服务器发起 [开发者测试号] 登录请求，等待回应中..."));

        // 【回调修复】：传入 LoginContext 作为 ClientData
        EOS_Auth_Login(AuthHandle, &LoginOptions, LoginContext, [](const EOS_Auth_LoginCallbackInfo* Data)
            {
                // 【核心防泄漏】：第一时间接管 ClientData 并转换为上下文指针
                FEOSLoginContext* Context = static_cast<FEOSLoginContext*>(Data->ClientData);

                // 【进阶加固 A】：极限防止关机死锁，确保 Manager 和 GameInstance 都健在才往下走
                if (!Context || !Context->Manager || !Context->Manager->GetGameInstance())
                {
                    if (Context) delete Context;
                    return;
                }

                // 提取我们安全的 UEOSManager 指针
                UEOSManager* Manager = Context->Manager;

                // 判断服务器返回的最终结果码是否为成功
                if (Data->ResultCode == EOS_EResult::EOS_Success)
                {
                    // 登录成功，打印底层通道模拟成功的确认日志
                    INJECTOR_LOG(Warning, TEXT(">> C-API 底层发号器模拟登录成功！ <<"));

                    // 准备一个字符数组用于接收 Epic Account ID 的字符串，长度预设为最大值加1作为结束符
                    char AccountIdString[EOS_EPICACCOUNTID_MAX_LENGTH + 1];
                    // 准备一个变量记录缓冲区大小
                    int32_t BufferSize = sizeof(AccountIdString);
                    // 调用 C-API 将底层非透明的 LocalUserId 句柄转换为可读的字符串 ID
                    if (EOS_EpicAccountId_ToString(Data->LocalUserId, AccountIdString, &BufferSize) == EOS_EResult::EOS_Success)
                    {
                        // 将转换出的 UTF-8 C 风格字符串包装回虚幻引擎的 FString
                        FString PUIDString = UTF8_TO_TCHAR(AccountIdString);
                        // 把真假状态明确地传给日志
                        INJECTOR_LOG(Warning, TEXT("【发号器测试号】(Developer) 登录成功！ PUID: %s"), *PUIDString);
                    }

                    if (Manager)
                    {
                        if (Manager->GetGameInstance())
                        {
                            // 在定时器管理器中注册一个循环执行的定时器，每 5 秒调用一次 CheckConnectionStatus
                            Manager->GetGameInstance()->GetTimerManager().SetTimer(Manager->ConnectionCheckTimer, Manager, &UEOSManager::CheckConnectionStatus, 5.0f, true);
                        }

                        // 【完美归位】：直接调用自身的同步函数去刷新 V2 账本
                        Manager->SyncCAPIUserToOSSv2();
                    }
                }
                else
                {
                    // 如果登录失败，提取底层返回的数字错误码，并将对应的枚举转换为字符串输出
                    INJECTOR_LOG(Error, TEXT("C-API 发号器登录失败！错误码: %d (EOS_Result: %s)"),
                        (int32)Data->ResultCode,
                        UTF8_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
                }

                // 【绝杀】：任务完毕，手动销毁堆内存上下文，干净利落绝不漏水！
                delete Context;
            });

        // 拦截完成，直接退出，绝不让 V2 碰我们的发号器参数！
        return;
    }

    // ==============================================================================
    // 正常匿名通道 (V2 DeviceID Logic)
    // ==============================================================================
    // 获取 V2 架构下的认证 (Auth) 接口指针，用于处理普通设备码登录
    IAuthPtr AuthInterface = OnlineServices->GetAuthInterface();
    // 如果指针无效，直接退出防止崩溃
    if (!AuthInterface.IsValid()) return;

    // 告知引擎将使用的凭据类型为外部授权 (ExternalAuth)
    Params.CredentialsType = LoginCredentialsType::ExternalAuth;

    // 创建一个用于携带设备码的外部凭据令牌结构体
    FExternalAuthToken DeviceToken;
    // 指定外部令牌的具体类型为设备 ID 访问令牌
    DeviceToken.Type = ExternalLoginType::DeviceIdAccessToken;
    // 由于是利用本地系统凭据中的设备码静默登录，所以需要传空字符串
    DeviceToken.Data = FString();

    // 将配置好的设备码令牌装入 Params 结构体，MoveTemp 用于避免深拷贝开销
    Params.CredentialsToken.Emplace<FExternalAuthToken>(MoveTemp(DeviceToken));

    // 只有在走真实设备码账号时，才清空 CredentialsId
    Params.CredentialsId = FString();

    // 真实匿名设备码登录不需要任何权限
    Params.Scopes = {};

    // 【加上这行】：在发起异步请求前，明确打印开始行动
    INJECTOR_LOG(Warning, TEXT("正在向 Epic 服务器发起 [匿名设备码] 登录请求，等待回应中..."));

    // 发起 V2 接口的异步登录请求，并绑定完成后的回调逻辑
    AuthInterface->Login(MoveTemp(Params))
        .OnComplete(this, [this](const TOnlineResult<FAuthLogin>& Result)
            {
                // 检查 Result 对象，看设备码登录是否被服务器放行
                if (Result.IsOk())
                {
                    // 从成功的返回结果中提取账号信息的共享引用
                    const TSharedRef<FAccountInfo> AccountInfo = Result.GetOkValue().AccountInfo;
                    // 从账号信息中提取出通用的 AccountId，并转为可以输出的字符串格式
                    FString PUIDString = ToLogString(AccountInfo->AccountId);

                    // 把真假状态明确地传给日志
                    INJECTOR_LOG(Warning, TEXT("【真实Epic号】(DeviceID) 登录成功！ PUID: %s"), *PUIDString);

                    // 检查当前的游戏实例是否存在
                    if (GetGameInstance())
                    {
                        // 启动断线监测的循环定时器
                        GetGameInstance()->GetTimerManager().SetTimer(ConnectionCheckTimer, this, &UEOSManager::CheckConnectionStatus, 5.0f, true);
                    }

                    // 【双轨广播】：真实玩家走正门，不用刷账本，直接广播准备就绪
                    OnUserReadyForLobby.Broadcast();
                }
                else
                {
                    // 登录失败，提取包含失败详情的错误对象
                    const FOnlineError& Error = Result.GetErrorValue();
                    // 获取具体的错误 ID 字符串
                    FString ErrorId = Error.GetErrorId();

                    // 判断是否因为本地凭据中完全没有设备码缓存导致的特定找不到错误
                    if (Error.GetLogString().Contains(TEXT("not_found")) || Error.GetLogString().Contains(TEXT("EOS_NotFound")) || ErrorId.Contains(TEXT("1.1.12")))
                    {
                        // 打印拦截日志，说明触发了无设备码的生成逻辑
                        INJECTOR_LOG(Warning, TEXT("[拦截成功] V2 报告本地无设备码，正在触发 C-API 底层生成..."));

                        // 再次获取 EOS SDK 管理器单例，准备调用 C-API
                        IEOSSDKManager* SDKManager = IEOSSDKManager::Get();
                        // 如果获取失败直接退出
                        if (!SDKManager) return;

                        // 获取当前活跃的所有平台句柄列表
                        TArray<IEOSPlatformHandlePtr> ActivePlatforms = SDKManager->GetActivePlatforms();
                        // 确保至少有一个有效的平台句柄
                        if (ActivePlatforms.Num() == 0 || !ActivePlatforms[0].IsValid()) return;

                        // 解引用获取底层的原生 C-API 平台句柄
                        EOS_HPlatform NativePlatform = *ActivePlatforms[0];
                        // 从原生句柄中请求获取 Connect (连接服务) 接口，该接口负责处理设备码相关逻辑
                        EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(NativePlatform);
                        // 如果没拿到连接句柄，安全退出
                        if (!ConnectHandle) return;

                        // 初始化创建设备 ID 的配置选项，全部清零
                        EOS_Connect_CreateDeviceIdOptions CreateOptions = { 0 };
                        // 指定 API 版本为所需的最新版本
                        CreateOptions.ApiVersion = EOS_CONNECT_CREATEDEVICEID_API_LATEST;
                        // 给设备指定一个型号名称 "PC"，用于 Epic 后台的标识记录
                        CreateOptions.DeviceModel = "PC";

                        // 呼叫 C-API 函数，向 Epic 服务器申请分配并本地保存一个新的匿名设备码
                        EOS_Connect_CreateDeviceId(ConnectHandle, &CreateOptions, this, [](const EOS_Connect_CreateDeviceIdCallbackInfo* Data)
                            {
                                // 判断请求创建的设备码是否成功拿到并写入了系统
                                if (Data->ResultCode == EOS_EResult::EOS_Success)
                                {
                                    // 创建成功，打印提示日志
                                    INJECTOR_LOG(Warning, TEXT(">> C-API 底层静默生成成功！正在重新唤起 V2 登录... <<"));

                                    // 将数据附带的指针转换回 UEOSManager 管理器自身
                                    UEOSManager* Manager = static_cast<UEOSManager*>(Data->ClientData);
                                    // 检查指针有效性
                                    if (Manager)
                                    {
                                        // 重新调用 StartEOSLogin 递归执行，有了刚刚生成的设备码，这次 V2 就能顺利登入了
                                        Manager->StartEOSLogin();
                                    }
                                }
                                else
                                {
                                    // 生成设备码失败，输出底层返回的错误码信息
                                    INJECTOR_LOG(Error, TEXT("C-API 生成设备码失败！错误码: %d"), (int32)Data->ResultCode);
                                }
                            });
                    }
                    else
                    {
                        // 遇到了与缺失设备码无关的其他类型报错，直接打印详细的错误原因
                        INJECTOR_LOG(Error, TEXT("V2 登录发生其他错误: %s"), *Error.GetLogString());
                    }
                }
            });
}

#pragma endregion

// ==============================================================================
// 凭据同步注册 (OSSv2 账本刷新)
// ==============================================================================
#pragma region

void UEOSManager::SyncCAPIUserToOSSv2()
{
    // 引入 OSSv2 的核心命名空间，避免到处写 UE::Online::
    using namespace UE::Online;

    // 1. 获取 OSSv2 接口
    IOnlineServicesPtr OnlineServices = GetServices(EOnlineServices::Default);
    // 安全拦截：如果在线服务接口未就绪，直接截断
    if (!OnlineServices.IsValid()) return;

    // 提取 V2 架构下的身份验证 (Auth) 接口
    IAuthPtr AuthInterface = OnlineServices->GetAuthInterface();
    // 安全拦截：如果 Auth 接口获取失败，直接截断
    if (!AuthInterface.IsValid()) return;

    // 2. 获取本地玩家 ID
    // 明确发起请求的物理设备（解决 V2 架构强制要求区分本地不同手柄/键鼠玩家的问题）
    FPlatformUserId LocalUser = IPlatformInputDeviceMapper::Get().GetPrimaryPlatformUser();

    // 3. 【关键步骤】：提取底层真实 ID Token 并发起同步登录
    // 告知 OSSv2 使用当前已存在的底层认证状态 (由于 EAS 关闭，此处需提取真实 Token 喂给 Connect)

    // 越权获取虚幻引擎底层的 EOS SDK 单例实例
    IEOSSDKManager* SDKManager = IEOSSDKManager::Get();
    // 安全拦截：防空指针
    if (!SDKManager) return;

    // 获取当前所有激活的平台句柄（剥开引擎的第一层封装）
    TArray<IEOSPlatformHandlePtr> ActivePlatforms = SDKManager->GetActivePlatforms();
    // 安全拦截：确保底层确实有被初始化的平台句柄
    if (ActivePlatforms.Num() == 0 || !ActivePlatforms[0].IsValid()) return;

    // 暴力解引用，掏出 Epic 官方 C-API 的 Auth 接口句柄 (EOS_HAuth)
    EOS_HAuth LocalAuthHandle = EOS_Platform_GetAuthInterface(*ActivePlatforms[0]);
    // 直接向 Epic C-API 索要当前索引为 0 的本地已登录账号
    EOS_EpicAccountId LoggedInAccountId = EOS_Auth_GetLoggedInAccountByIndex(LocalAuthHandle, 0);

    // 查验 C-API 是否真的有已登录的账号
    if (!LoggedInAccountId)
    {
        INJECTOR_LOG(Error, TEXT("❌ OSSv2 同步失败：底层 C-API 找不到已登录的账号！"));
        return;
    }

    // 提取 ID Token
    // 初始化 C-API 的 Token 提取参数结构体并清零内存
    EOS_Auth_CopyIdTokenOptions IdOptions = { 0 };
    // 强制声明使用的 API 版本，防止 EOS SDK 升级导致结构体错位
    IdOptions.ApiVersion = EOS_AUTH_COPYIDTOKEN_API_LATEST;
    // 指定要提取 Token 的目标账号
    IdOptions.AccountId = LoggedInAccountId;

    // 准备一个空指针，用于接收底层分配的 Token 内存块
    EOS_Auth_IdToken* IdToken = nullptr;
    // 准备一个虚幻字符串，用于承接转码后的 Token 数据
    FString ExtractedTokenStr;

    // 发起同步提取请求，并检查返回值是否为绝对成功 (EOS_Success) 且指针不为空
    if (EOS_Auth_CopyIdToken(LocalAuthHandle, &IdOptions, &IdToken) == EOS_EResult::EOS_Success && IdToken)
    {
        // 【进阶加固 B】：使用 5.8 规范的 RAII 机制，确保哪怕后续代码断崖式跳出，句柄也会被绝对释放！
        ON_SCOPE_EXIT{ EOS_Auth_IdToken_Release(IdToken); };

        // 将 C-API 返回的 UTF-8 格式 JWT 字符串，安全转码为虚幻引擎的宽字符 TCHAR
        ExtractedTokenStr = UTF8_TO_TCHAR(IdToken->JsonWebToken);
    }
    else
    {
        INJECTOR_LOG(Error, TEXT("❌ OSSv2 同步失败：无法提取 ID Token！"));
        return;
    }

    // 组装 V2 架构所需的登录参数结构体
    FAuthLogin::Params LoginParams;
    // 绑定刚刚获取的本地物理玩家句柄
    LoginParams.PlatformUserId = LocalUser;

    // 改用 ExternalAuth 携带提取到的 Token 进行登录验证
    LoginParams.CredentialsType = LoginCredentialsType::ExternalAuth;

    // 声明一个外部凭据包装器
    FExternalAuthToken EpicToken;
    // ==============================================================================
    // 【核心修正】：将标签从 Epic 改为 EpicIdToken，解决 Token type mismatch 报错！
    // ==============================================================================
    EpicToken.Type = ExternalLoginType::EpicIdToken;

    // 将刚才提取并转码的底层 JWT 字符串塞进凭据中
    EpicToken.Data = ExtractedTokenStr;
    // 使用 Emplace 完美匹配 V2 架构的 TVariant 变体类型参数，避免拷贝损耗
    LoginParams.CredentialsToken.Emplace<FExternalAuthToken>(MoveTemp(EpicToken));

    INJECTOR_LOG(Warning, TEXT("正在将 C-API 凭据同步至 OSSv2 账本..."));

    // 创建弱指针，防止在异步回调等待期间当前类被销毁导致野指针崩溃
    TWeakObjectPtr<UEOSManager> WeakSelf(this);

    // 正式向 OSSv2 发起一轮“伪造登录”，强行刷入账本
    AuthInterface->Login(MoveTemp(LoginParams))
        .OnComplete(this, [WeakSelf](const TOnlineResult<FAuthLogin>& Result)
            {
                // 【补丁 A 应用：极限内存防御锁】
                // IsValid 只能判定内存未被系统抹除，HasAnyFlags 判定当前是否正在被垃圾回收(GC)无情处决
                if (WeakSelf.IsValid() && !WeakSelf->HasAnyFlags(RF_BeginDestroyed))
                {
                    // 检查 V2 账本同步是否成功
                    if (Result.IsOk())
                    {
                        // 此时 OSSv2 终于“承认”了该用户
                        // 从 V2 返回的结构体中萃取出合法的 FAccountId
                        FAccountId RegisteredId = Result.GetOkValue().AccountInfo->AccountId;
                        INJECTOR_LOG(Warning, TEXT("✅ OSSv2 账本同步成功！ID: %s"), *ToLogString(RegisteredId));

                        // 广播通知：V2 大厅系统现在可以放行了！
                        // 扣动信号枪，通知 UI 或 LobbySubsystem 开始执行后续联网逻辑
                        WeakSelf->OnUserReadyForLobby.Broadcast();
                    }
                    else
                    {
                        // 探针抓取 V2 拒绝同步的具体错误详情并打印
                        INJECTOR_LOG(Error, TEXT("❌ OSSv2 同步失败，大厅功能将不可用: %s"),
                            *Result.GetErrorValue().GetLogString());
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
    // 打印一行基础日志，用于在外部调用时验证诊断模块和注入器宏是否配置正确且正常工作
    INJECTOR_LOG(Log, TEXT("诊断模块就绪。"));
}

void UEOSManager::CheckConnectionStatus()
{
    // 获取 EOS SDK 管理器单例指针，用来访问底层的引擎在线服务实例
    IEOSSDKManager* SDKManager = IEOSSDKManager::Get();
    // 若指针无效则直接跳过本次检查，防止出现空指针崩溃异常
    if (!SDKManager) return;

    // 拉取当前处于活动状态的所有 EOS 平台句柄数组
    TArray<IEOSPlatformHandlePtr> ActivePlatforms = SDKManager->GetActivePlatforms();
    // 确认数组内有元素，并且第一个平台的句柄是有效的
    if (ActivePlatforms.Num() == 0 || !ActivePlatforms[0].IsValid()) return;

    // 解引用平台句柄获取到原生的 EOS C-API 句柄 (EOS_HPlatform)
    EOS_HPlatform NativePlatform = *ActivePlatforms[0];
    // 核心：调用零网络消耗的 C-API 函数，从本地缓存直接读取当前的真实网络连接状态
    EOS_ENetworkStatus NetworkStatus = EOS_Platform_GetNetworkStatus(NativePlatform);

    // 将获取到的网络状态枚举转为 int，并通过注入器的专门心跳函数来进行屏幕和日志的打印分发
    FEOSTestInjector::LogHeartbeat((int32)NetworkStatus);

    // ==============================================================================
    // 【未来架构方向：进阶加固 C - 超时断线防爆破与强制踢人机制】
    // TODO: 
    // 1. 在这里拦截并判断 (NetworkStatus == EOS_ENetworkStatus::EOS_NS_NoNetworkConnection)
    // 2. 一旦捕获断线，立刻触发静默重试逻辑 (设置 3 次失败阈值)。
    // 3. 如果超过重试阈值，必须强行截断 UI，调起 MyLobbySubsystem 的解散/离线接口，
    //    同时调用引擎级的 ClientReturnToMainMenu 强制清空玩家状态并踢回主界面！
    //    (注：防 Iris 崩溃可配合调用 GEngine->HandleDisconnect)
    // ==============================================================================
}

#pragma endregion
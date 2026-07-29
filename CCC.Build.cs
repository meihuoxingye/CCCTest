// Fill out your copyright notice in the Description page of Project Settings.

using System.IO;
using UnrealBuildTool;

public class CCC : ModuleRules
{
	public CCC(ReadOnlyTargetRules Target) : base(Target)
	{
        // 1. 开启 IWYU (Include What You Use) 全力支持
        // 在 C++23 下，严格的头文件管理能极大缩减 VS 2026 的编译时间
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        IWYUSupport = IWYUSupport.Full;

        // 2. 【核心修改】：强制开启 C++23 标准
        // 配合 VS 2026，这允许你在 C++ 中使用最新的语言特性
        CppStandard = CppStandardVersion.Cpp23;

        PublicDependencyModuleNames.AddRange(new string[]
{ 
    // ==========================================
    // 核心与引擎基础 (Core & Engine Base)
    // ==========================================
    "Core",
    "CoreUObject",
    "Engine",
    "InputCore",
    "ApplicationCore", // 【补在这里！】提供 IPlatformInputDeviceMapper 等底层平台与硬件设备映射支持
    "DeveloperSettings", // <--- 【加在这里！】提供 UDeveloperSettings 支持，用于项目设置面板展示

    // ==========================================
    // 渲染相关:底层渲染劫持 (Rendering)
    // ==========================================
    "RenderCore",
    "RHI",

    // ==========================================
    // UI 与 视图模型 (UI & MVVM)
    // ==========================================
    "UMG",
    "Slate",
    "SlateCore",
    "ModelViewViewModel",
    "CommonUI",
    "CommonInput",

    // ==========================================
    // 玩法、AI 与输入 (Gameplay, AI & Input)
    // ==========================================
    "EnhancedInput",
    "AIModule",
    "NavigationSystem",
    "GameplayTags",

    // ==========================================
    // 数据解析与工具 (Data & Utilities)
    // ==========================================
    "Json",
    "JsonUtilities",

    // ==========================================
    // 联机与 EOS v2 框架 (Online Services)
    // ==========================================
    "CoreOnline",              // 基础在线类型支持
    "OnlineServicesInterface", // 新版接口定义
    "OnlineServicesEOSGS",      // EOS 游戏服务支持,不强迫玩家注册 Epic 账号
    // ==========================================
    // 越狱底层 C-API 必需模块
    // ==========================================
    "EOSSDK",                  // 核心命脉：让编译器认识 eos_connect.h 里的原生 C 函数和结构体
    "EOSShared",               // 桥梁命脉：让编译器认识 IEOSSDKManager 和 IEOSPlatformHandle 封装类

});

        // Uncomment if you are using Slate UI
        // PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

        // Uncomment if you are using online features
        // PrivateDependencyModuleNames.Add("OnlineSubsystem");

        // To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
    }
}

// Fill out your copyright notice in the Description page of Project Settings.

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

        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "AIModule", "NavigationSystem", "UMG", "Slate", "SlateCore", "ModelViewViewModel", "Json", "JsonUtilities", "CommonUI", "CommonInput","GameplayTags","RenderCore", "RHI" });

		PrivateDependencyModuleNames.AddRange(new string[] {  });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}

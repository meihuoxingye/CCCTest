// Fill out your copyright notice in the Description page of Project Settings.

#include "Tools/MyUITools.h"
#include "Engine/World.h"

// 整个虚幻引擎 Slate UI 系统的“大总管”（全局单例）
// 无论是 UMG 还是编辑器里的窗口，所有鼠标点击、键盘输入、焦点转移，全部都要经过它
#include "Framework/Application/SlateApplication.h" 
// UI 路径树结构体。当鼠标点击屏幕时，它负责记录到底戳穿了多少层 UI
#include "Layout/WidgetPath.h"

// 修复 GEngine 未识别报错，引入虚幻核心引擎头文件
#include "Engine/Engine.h"


// ==============================================================================
// UI 检测工具 (UI Detection Tools)
// ==============================================================================
#pragma region
bool UMyUITools::IsMouseOverUI(const UObject* WorldContextObject)
{
	// 1. 【静态环境溯源】：
	// 由于本类是全局静态工具库 (UMyUITools), 它脱离了具体的关卡，没有自己的状态，也无法直接调用 GetWorld()
	// 所以必须由调用方 (如 TopCharacter) 将自身作为上下文 (WorldContextObject) 传进来
	// 充当“指路明灯”，帮助工具函数顺藤摸瓜找到当前游戏世界的 UWorld 内存地址
	if (!WorldContextObject || !WorldContextObject->GetWorld()) return false;

	// ===================== 【终极防走火检测机制】 =====================
	bool bIsOverUI = false;

	// 【底层防线：Slate 底层几何体追踪 (引擎级穿透)】
	// 唤醒虚幻的“UI 大总管” —— FSlateApplication 全局单例
	if (FSlateApplication::IsInitialized())
	{
		// LocateWindowUnderMouse：相当于从屏幕向内发射一条极其精确的“2D 物理射线”，穿透所有的 UI 图层
		// 参数1：获取当前操作系统的真实鼠标光标位置
		// 参数2：获取当前所有可交互的顶层窗口列表
		FWidgetPath WidgetPath = FSlateApplication::Get().LocateWindowUnderMouse(
			FSlateApplication::Get().GetCursorPos(),
			FSlateApplication::Get().GetInteractiveTopLevelWindows()
		);

		// 如果射线命中了一串 UI 控件 (例如命中链路：CanvasPanel -> Overlay -> SButton)
		if (WidgetPath.IsValid() && WidgetPath.Widgets.Num() > 0)
		{
			// 拿到这条射线打到的【最表层叶子节点】的底层 C++ 类型名
			// 注意：在 UMG 蓝图里叫 Button、Image，但在 C++ 的 Slate 底层，它们的名字叫 SButton、SImage
			FString LeafWidgetName = WidgetPath.Widgets.Last().Widget->GetTypeAsString();

			// 屏幕显示调试信息：鼠标点到了哪个 UI 控件上（底层 C++ 类型名）
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, FString::Printf(TEXT("鼠标点到了: %s"), *LeafWidgetName));

			// 【绝对核心判断逻辑】：
			// 虚幻引擎的 3D 游戏画面，本质上也是一个铺满全屏的底板 UI 控件！它的名字就叫 "SViewport"
			// 此外，UMG 蓝图默认自带全屏透明画布 "SConstraintCanvas"，引擎底层还有 "SGameLayerManager"
			// 以及包裹所有用户蓝图控件的底层透明外壳 "SObjectWidget"
			// 如果射线最终打到的是这四个，说明此时鼠标悬空在纯粹的游戏世界（比如地面）上
			// 如果打到的名字不是它们（而是 SImage, STextBlock 等任何东西），说明肯定点在了真 UI 上
			if (LeafWidgetName != TEXT("SViewport") &&
				LeafWidgetName != TEXT("SConstraintCanvas") &&
				LeafWidgetName != TEXT("SGameLayerManager") &&
				LeafWidgetName != TEXT("SObjectWidget"))
			{
				bIsOverUI = true;
			}
		}
	}

	return bIsOverUI;
}
#pragma endregion
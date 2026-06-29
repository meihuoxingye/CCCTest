#include "MapTravel/MyTravelSessionSubsystem.h"

// ==============================================================================
// 全局状态获取与清理 (State Retrieval & Cleanup)
// ==============================================================================
#pragma region

UClass* UMyTravelSessionSubsystem::ConsumeLoadingClass()
{
	if (PendingLoadingWidgetClass.IsNull())
	{
		return nullptr;
	}

	// 1. 尝试从软引用中直接获取已加载的类（极大缓解同步加载引起的掉帧）
	UClass* LoadedClass = PendingLoadingWidgetClass.Get();

	// 2. 如果资产尚未载入（比如从内存中被剔除了），执行同步加载作为最后保险
	if (!LoadedClass)
	{
		LoadedClass = PendingLoadingWidgetClass.LoadSynchronous();
	}

	// 3. 执行“消费”逻辑，确保状态不残留，彻底消除状态污染风险
	PendingLoadingWidgetClass = nullptr;

	return LoadedClass;
}

#pragma endregion
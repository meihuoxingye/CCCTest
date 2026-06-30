#include "MapTravel/MyTravelSessionSubsystem.h"

// ==============================================================================
// 全局状态获取与清理 (State Retrieval & Cleanup)
// ==============================================================================
#pragma region

UClass* UMyTravelSessionSubsystem::PeekLoadingClass()
{
	if (PendingLoadingWidgetClass.IsNull()) return nullptr;

	UClass* LoadedClass = PendingLoadingWidgetClass.Get();
	if (!LoadedClass)
	{
		LoadedClass = PendingLoadingWidgetClass.LoadSynchronous();
	}

	return LoadedClass;
}

UClass* UMyTravelSessionSubsystem::ConsumeLoadingClass()
{
	// 内部复用 Peek 逻辑
	UClass* LoadedClass = PeekLoadingClass();

	// 拿完立刻清空，绝不残留
	PendingLoadingWidgetClass = nullptr;

	return LoadedClass;
}

#pragma endregion
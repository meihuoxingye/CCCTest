#include "MapTravel/MyTravelSessionSubsystem.h"

// ==============================================================================
// 全局状态获取与清理 (State Retrieval & Cleanup)
// ==============================================================================
#pragma region

UClass* UMyTravelSessionSubsystem::GetValidLoadingClassAndCleanup()
{
	UClass* ResultClass = nullptr;

	if (!PendingLoadingWidgetClass.IsNull())
	{
		ResultClass = PendingLoadingWidgetClass.LoadSynchronous();
	}

	// 【核心】：拿完货立刻清空，绝不残留
	PendingLoadingWidgetClass = nullptr;

	return ResultClass;
}

#pragma endregion
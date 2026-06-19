#include "UI/CommonButton/MyCommonButtonBase.h" 
#include "CommonTextBlock.h"

// ==============================================================================
// 自定义 CommonUI 按钮基类
// ==============================================================================
#pragma region

void UMyCommonButtonBase::NativePreConstruct()
{
	Super::NativePreConstruct();

	// 把你在编辑器右侧面板里敲的字，实时显示到画面上的文本块里
	if (Text_ButtonName)
	{
		Text_ButtonName->SetText(DefaultButtonText);
	}
}

void UMyCommonButtonBase::SetButtonText(const FText& InText)
{
	// 游戏运行时，真正执行改字的逻辑
	if (Text_ButtonName)
	{
		Text_ButtonName->SetText(InText);
	}
}

#pragma endregion
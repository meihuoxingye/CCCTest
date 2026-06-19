#include "UI/CommonButton/MyCommonButtonBase.h"
#include "CommonTextBlock.h" // 引入 CommonUI 专属文本块头文件
#include "Components/ButtonSlot.h"

// ==============================================================================
// 生命周期与核心虚函数 (Lifecycle & Core Virtual Functions)
// ==============================================================================
#pragma region

void UMyCommonButtonBase::NativePreConstruct()
{
	Super::NativePreConstruct();

	// 防御性校验，确保蓝图里真的绑定了文本块
	if (Text_ButtonName)
	{
		// 1. 动态读取外部面板设置的对齐方式，彻底接管排版权
		Text_ButtonName->SetJustification(ButtonTextJustification);

		if (UButtonSlot* BtnSlot = Cast<UButtonSlot>(Text_ButtonName->Slot))
		{
			// 动态读取插槽水平对齐（支持靠左、居中、靠右等一切选项），用 C++ 强行物理锁死
			BtnSlot->SetHorizontalAlignment(ButtonHorizontalAlignment);

			// 动态读取插槽垂直对齐
			BtnSlot->SetVerticalAlignment(ButtonVerticalAlignment);
		}

		// 2. 【自动投射逻辑】：有字才赋值，空着就不碰（防止空字符重置文本框尺寸）
		if (!DefaultButtonText.IsEmpty())
		{
			Text_ButtonName->SetText(DefaultButtonText);
		}
	}
}

#pragma endregion
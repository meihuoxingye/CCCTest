#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"

#include "Framework/Text/TextLayout.h" // 【新增】：告诉编译器 ETextJustify 是什么

#include "MyCommonButtonBase.generated.h"

// 前向声明 CommonUI 专属文本块
class UCommonTextBlock;

// ==============================================================================
// UI 统筹系统 (UI Management System)
// ==============================================================================

UCLASS()
class CCC_API UMyCommonButtonBase : public UCommonButtonBase
{
	GENERATED_BODY()

	// ==============================================================================
	// 生命周期与核心虚函数 (Lifecycle & Core Virtual Functions)
	// ==============================================================================
protected:
	virtual void NativePreConstruct() override;


	// ==============================================================================
	// 控件配置参数 (Widget Configuration Parameters)
	// ==============================================================================
protected:

	// 【核心文本设置】：暴露给大卡片（外部）的文字输入框
	// 写什么就实时投射什么，不写就不会覆盖内部的测试文字
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Button")
	FText DefaultButtonText;

	// 暴露给外层面板的“水平对齐方式”（默认居中）
	// 【设立此变量的核心原因】：
	// 1. 规避引擎 Bug：UMG 面板在执行 NativePreConstruct 时，经常会发癫重置内部插槽的对齐设置。
	// 2. 保证组件复用性：如果在 C++ 底层把对齐方式写死为“居中”，这个模板就废了。
	// 将排版权限强制上交并暴露给最外层的细节面板，既能防止蓝图面板反复抽风重置，
	// 又能让外部自由定制（如：存档界面的按钮居中，而主菜单的按钮可以选靠左），实现真正的组件化。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Button")
	TEnumAsByte<EHorizontalAlignment> ButtonHorizontalAlignment = HAlign_Center;

	// 垂直对齐方式（默认居中）
	// 【设立此变量的核心原因】：
	// 赋予组件全方位的排版自由，防止后续需求变更（如特殊偏移布局）时无法通过细节面板快速调整。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Button")
	TEnumAsByte<EVerticalAlignment> ButtonVerticalAlignment = VAlign_Center;

	// 暴露给外层面板的“文本内部对齐方式”（默认居中）
	// 【设立此变量的核心原因】：
	// 与水平对齐同理，彻底夺回文本在其边框内部的对齐控制权。
	// 确保不论引擎的 Slate 底层如何重绘控件，这道 C++ 的绝对指令都能把文本钉在我们想要的位置。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Button")
	TEnumAsByte<ETextJustify::Type> ButtonTextJustification = ETextJustify::Center;


	// ==============================================================================
	// 控件绑定与交互逻辑 (Widget Binding & Interaction Logic)
	// ==============================================================================
protected:

	// 【严格遵守规范】：使用 CommonUI 的核心文本块
	// 必须在蓝图中创建一个名为 "Text_ButtonName" 的 UCommonTextBlock 组件，
	// 否则 C++ 层面的 BindWidget 会因找不到控件而导致编译失败。
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "UI|Button")
	TObjectPtr<UCommonTextBlock> Text_ButtonName;
};
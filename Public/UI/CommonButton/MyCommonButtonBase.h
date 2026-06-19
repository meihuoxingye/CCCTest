#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "MyCommonButtonBase.generated.h"

// ==============================================================================
// 自定义 CommonUI 按钮基类
// ==============================================================================

UCLASS(Abstract)
class CCC_API UMyCommonButtonBase : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	// 给 C++ 或蓝图在游戏运行时，动态修改按钮文字用的函数
	UFUNCTION(BlueprintCallable, Category = "UI|Button")
	void SetButtonText(const FText& InText);

protected:
	// 预构造：让代码在 UE 编辑器里（不需要点运行）就能执行，用于画面实时预览
	virtual void NativePreConstruct() override;

	// 暴露给蓝图的变量：让你能在编辑器右侧的细节面板里直接填初始文字
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Button")
	FText DefaultButtonText;

	// C++ 强行认主：绑定蓝图里的同名文本控件
	// 警告：继承此类的蓝图里，必须有一个叫 Text_ButtonName 的 CommonTextBlock，否则报错
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "UI|Button")
	TObjectPtr<class UCommonTextBlock> Text_ButtonName;
};
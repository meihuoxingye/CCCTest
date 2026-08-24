#pragma once

#include "CoreMinimal.h"
#include "UI/Transition/MyTransitionWidgetBase.h"
#include "MyScreenOffWidget.generated.h"

// 声明熄屏 UI 入场动画彻底播完（屏幕完全闭合黑透）时的专属多播委托
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnScreenOffCoveredSignature);

UCLASS()
class CCC_API UMyScreenOffWidget : public UMyTransitionWidgetBase
{
	GENERATED_BODY()

public:
	// 【架构解耦补充】：入场动画彻底结束（黑透）时广播！(瞎子对外呼叫，供大管家监听并向服务器发送握手信号)
	UPROPERTY(BlueprintAssignable, Category = "Transition|Event")
	FOnScreenOffCoveredSignature OnScreenOffCovered;

	// 熄屏 UI 专属物理拆卸接口,停动画防 GC 咬死强引用并从底层渲染树物理抹杀自身
	// 由大管家 (GameInstance) 在同图流送就绪或跨图漫游起航时强制调用。
	UFUNCTION(BlueprintCallable, Category = "TransitionUI|Lifecycle")
	void DismissScreenOffUI();

protected:
	// 【认领基类钩子】：重写基类的入场动画结束钩子，用于精准发射黑透广播
	virtual void OnOpeningAnimationFinished() override;
};
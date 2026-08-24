// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Transition/MyScreenOffWidget.h"


void UMyScreenOffWidget::DismissScreenOffUI()
{
	// 💥【采纳优化 3：斩断动画强引用】：UMG 播放动画时直接 Remove 会被 Latent Action 死死咬住。
	// 处决前必须强停所有动画，彻底消灭 "Object from PIE level still referenced" 闪退巨坑！
	StopAllAnimations();

	// 彻底结束屏幕霸权，从底层渲染树物理抹杀自身
	RemoveFromParent();
}

void UMyScreenOffWidget::OnOpeningAnimationFinished()
{
	// 调用父类，保留原有的 bIsWaitingForEngine 状态机挂起逻辑
	Super::OnOpeningAnimationFinished();

	// 【熄屏专属业务补充】：入场动画彻底结束，此时屏幕已彻底黑透！
	// 对外无脑广播，大管家听到后会负责联络传送子系统，向服务器发射 Ack 握手信号
	OnScreenOffCovered.Broadcast();
}
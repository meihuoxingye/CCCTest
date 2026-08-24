// Fill out your copyright notice in the Description page of Project Settings.

#include "PlayerState/MyPlayerState.h"
// 注意：引入你即将重构的组件头文件（路径必须与之后放置的路径绝对一致）
#include "PlayerState/Component/TravelAndStreaming/MyMapTravelStateComponent.h"

AMyPlayerState::AMyPlayerState()
{
	// 【核心基建】：开启网络同步，允许将本载体以及挂载在身上的组件状态广播给所有客机
	bReplicates = true;

	// 【架构级焊死】：在 C++ 构造函数 (CDO阶段) 直接把传送组件死死焊在 PlayerState 上！
	// 这彻底消除了蓝图拼装带来的加载时序不可控和空指针崩溃风险，性能也最高。
	MapTravelComponent = CreateDefaultSubobject<UMyMapTravelStateComponent>(TEXT("MapTravelComponent"));

	// 注：由于 UMyMapTravelStateComponent 内部写了 SetIsReplicatedByDefault(true)，
	// 引擎会自动处理该组件的网络同步，这里无需再额外配置。
}
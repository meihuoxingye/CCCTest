// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CharacterSwitchSubsystem.generated.h"

class ATopCharacter;

// 1. 换人请求委托：UI 点击时广播，控制器接收
DECLARE_MULTICAST_DELEGATE_OneParam(FOnSwitchCharacterRequestSignature, ATopCharacter* /*TargetCharacter*/);

// 2. 主控角色变更委托：换人成功后广播，所有 UI 接收以刷新高亮
DECLARE_MULTICAST_DELEGATE_OneParam(FOnActiveCharacterChangedSignature, ATopCharacter* /*NewActiveChar*/);

UCLASS()
class CCC_API UCharacterSwitchSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/*
	// 换人频道的双向总线
	// 架构精髓：将“意图 (Request)”与“事实 (Changed)”严格拆分为双频道
	// 绝不允许 UI 自行决定状态，所有换人逻辑必须形成“UI申请 -> 引擎仲裁 -> 全局派发结果”的闭环

	// 架构核心拷问：为什么要用两个频道？
	// 答：为了彻底杜绝“状态撕裂 (State Tearing)” Bug。
	// 如果只有一个频道，UI 一点就亮，但万一底层逻辑(因角色阵亡/受控)拒绝了换人，就会导致 UI 亮着但人没换
	// 因此，必须建立严格的【UI盲发申请 -> 引擎底层仲裁 -> 全局下发既定事实】的单向审批流
	// UI 绝不能因为“玩家按了鼠标”而改变表现，只能因为“世界真正发生了改变”才改变表现
	*/


	// 【频道一：上行提议 (Command)】流向：UI -> 玩家控制器
	// 职责：传递玩家的“换人愿望”。
	// 机制：UI 点击后仅向此频道发送请求，发完后按兵不动（绝不提前修改高亮）
	// 仲裁：玩家控制器接听此频道，校验目标角色状态。若不可换则静默抛弃，若可换则执行 Possess
	FOnSwitchCharacterRequestSignature OnSwitchRequest;

	// 【频道二：下行事实 (Event/Fact)】流向：玩家控制器 -> 全宇宙 (含 UI / 摄像机等)
	// 职责：宣判引擎底层的“既定事实”
	// 机制：控制器在完成真正的灵魂交接后，向此频道广播“新王登基”
	// 执行：只有听到这个频道的广播，UI 才被允许更新高亮，摄像机才开始转移焦点
	// 意义：确保了逻辑层与表现层的 100% 绝对同步
	FOnActiveCharacterChangedSignature OnActiveCharacterChanged;


	// 交互鲁棒性：鼠标是否正悬停在任何换人 UI 上的全局标志位
	// 鲁棒性是系统在异常或极限情况下的“抗造能力”
	// 极限情况：鼠标刚移到 UI 上就开枪
	// 脆弱的做法：在开枪的那一瞬间，才去问 UI 系统“鼠标在不在你身上？”（这时候如果 UI 系统卡了，就完蛋了）
	// 鲁棒的做法：在开枪的几毫秒前，UI 就已经把“鼠标在我身上”这个结果写在黑板上了。开枪时只看黑板，不问 UI
	bool bIsPointerOverUI = false;
};
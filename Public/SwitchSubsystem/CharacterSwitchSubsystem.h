// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CharacterSwitchSubsystem.generated.h"

class ATopCharacter;


// 【架构演进记录 (Deprecated)】：原“频道一：换人请求委托 (FOnSwitchCharacterRequestSignature)” 已被彻底废除！
// 废除原因：换人请求属于 UI 与特定控制器之间的私密交互。全局子系统广播缺乏目标指向性，极易在多手柄/分屏模式下引发指令交叉与逻辑灾难。
// 现行规范：全面引入 IMyPlayerUIInterface 接口通信。利用接口“一对一精准制导”的底层特性，确保 UI 的请求仅能点对点送达给拥有它的专属 Controller，实现指令的绝对物理隔离。

// 主控角色变更委托：换人成功后广播，所有 UI 接收以刷新高亮
DECLARE_MULTICAST_DELEGATE_OneParam(FOnActiveCharacterChangedSignature, ATopCharacter* /*NewActiveChar*/);

UCLASS()
class CCC_API UCharacterSwitchSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	/*
	 * 【换人通信架构规范 (CQRS 演进版)】
	 * 绝不允许 UI 自行决定表现状态，所有换人逻辑必须形成绝对的单向闭环。
	 * * ❌ 【已废弃的频道一：上行提议 (Command)】
	 * 严禁在此子系统中处理 UI 的“意图（Request）”。所有的换人请求必须由 UI 通过接口点对点直发给控制器。
	 * * ✅ 【硕果仅存的下行事实 (Event/Fact)】
	 * 架构精髓：本子系统仅作为引擎底层的“最高法庭”，只负责宣判“既定事实”。
	 */

	// 【下行事实频道】
	// 流向：玩家控制器 OnPossess()广播站 -> UI_StatusWidget HandleActiveCharacterChanged(ATopCharacter* NewActiveChar)委托绑定函数
	// 职责：宣判引擎底层的“既定事实”
	// 机制：控制器在完成真正的灵魂交接后，向此频道广播“新王登基”
	// 执行：只有听到这个频道的广播，UI 才被允许更新高亮，摄像机才开始转移焦点
	// 意义：确保了逻辑层与表现层的 100% 绝对同步
	FOnActiveCharacterChangedSignature OnActiveCharacterChanged;
};
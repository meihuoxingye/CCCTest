// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MySavableInterface.generated.h"

UINTERFACE(MinimalAPI)
class UMySavableInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 3A 级万能存读档接口
 * 任何挂载了此接口的子系统，都能被大管家自动识别并进行存盘调度
 */
class CCC_API IMySavableInterface
{
	GENERATED_BODY()

public:
	// 规定 1：交出你的模块名 (比如 "SkillPointSystem")，以此作为存盘的唯一 Key
	virtual FName GetModuleName() const = 0;

	// 规定 2：把你那复杂的业务数据，压缩成一段毫无逻辑的通用 JSON 字符串交给我
	virtual FString ExtractUniversalData() = 0;

	// 规定 3：读档时，我会塞给你一段属于你的 JSON 字符串，你自己去负责解析还原
	virtual void InjectUniversalData(const FString& InJSONString) = 0;
};
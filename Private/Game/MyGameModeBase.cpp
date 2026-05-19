// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/MyGameModeBase.h"

//角色
#include "Character/TopCharacter.h"


void AMyGameModeBase::RegisterFriendly(ATopCharacter* Character)
{
	if (Character)
	{
		// 在数组里从头到尾扫一遍，看看有没有一模一样的指针，没有则塞入
		FriendlyRoster.AddUnique(Character);
	}
}

void AMyGameModeBase::UnregisterFriendly(ATopCharacter* Character)
{
	if (Character)
	{
		// 后续
		// 把死掉的角色留在队列里“灰色显示”，不需要频繁变动数组
		// 放弃冷冰冰的系统内置排序，可以让玩家自主手动调整位置
		FriendlyRoster.Remove(Character);
	}
}

// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/MyGameModeBase.h"

//角色
#include "Character/TopCharacter.h"


void AMyGameModeBase::RegisterFriendly(ATopCharacter* Character)
{
	if (Character)
	{
		FriendlyRoster.AddUnique(Character);
	}
}

void AMyGameModeBase::UnregisterFriendly(ATopCharacter* Character)
{
	if (Character)
	{
		FriendlyRoster.Remove(Character);
	}
}

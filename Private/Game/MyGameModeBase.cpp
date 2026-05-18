// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/MyGameModeBase.h"

void AMyGameModeBase::RegisterFriendly(ABaseCharacter* Character)
{
	if (Character)
	{
		FriendlyRoster.AddUnique(Character);
	}
}

void AMyGameModeBase::UnregisterFriendly(ABaseCharacter* Character)
{
	if (Character)
	{
		FriendlyRoster.Remove(Character);
	}
}

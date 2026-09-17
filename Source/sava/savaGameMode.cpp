// Copyright Epic Games, Inc. All Rights Reserved.

#include "savaGameMode.h"
#include "savaCharacter.h"
#include "UObject/ConstructorHelpers.h"

AsavaGameMode::AsavaGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

}

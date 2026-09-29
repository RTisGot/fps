// Copyright Epic Games, Inc. All Rights Reserved.

#include "savaGameMode.h"
#include "savaCharacter.h"
#include "Player/SavaPlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "UObject/ConstructorHelpers.h"

AsavaGameMode::AsavaGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

	//能力とチームを持つ PlayerState を使う
	PlayerStateClass = ASavaPlayerState::StaticClass();
}

void AsavaGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	ASavaPlayerState* NewPlayerState = NewPlayer ? NewPlayer->GetPlayerState<ASavaPlayerState>() : nullptr;
	if (!NewPlayerState || !GameState)
	{
		return;
	}

	//人数の少ないチームに入れる
	TArray<int32> TeamCounts;
	TeamCounts.SetNumZeroed(NumTeams);
	for (const APlayerState* PlayerState : GameState->PlayerArray)
	{
		const ASavaPlayerState* SavaPlayerState = Cast<ASavaPlayerState>(PlayerState);
		if (SavaPlayerState && SavaPlayerState != NewPlayerState && TeamCounts.IsValidIndex(SavaPlayerState->GetTeamId()))
		{
			TeamCounts[SavaPlayerState->GetTeamId()]++;
		}
	}

	int32 SmallestTeam = 0;
	for (int32 TeamIndex = 1; TeamIndex < TeamCounts.Num(); ++TeamIndex)
	{
		if (TeamCounts[TeamIndex] < TeamCounts[SmallestTeam])
		{
			SmallestTeam = TeamIndex;
		}
	}
	NewPlayerState->SetTeamId(static_cast<uint8>(SmallestTeam));
}

// Fill out your copyright notice in the Description page of Project Settings.

#include "CTF/SavaGameState.h"
#include "CTF/SavaFlag.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

ASavaGameState::ASavaGameState()
{
	PrimaryActorTick.bCanEverTick = true; //デバッグ表示用
}

void ASavaGameState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

#if !UE_BUILD_SHIPPING
	if (bShowDebugText && GetNetMode() != NM_DedicatedServer)
	{
		DrawDebugText();
	}
#endif
}

float ASavaGameState::GetPhaseTimeRemaining() const
{
	if (PhaseEndServerTime <= 0.0f)
	{
		return 0.0f;
	}
	return FMath::Max(PhaseEndServerTime - GetServerWorldTimeSeconds(), 0.0f);
}

int32 ASavaGameState::GetTeamRoundWins(uint8 TeamId) const
{
	return TeamStates.IsValidIndex(TeamId) ? TeamStates[TeamId].RoundWins : 0;
}

bool ASavaGameState::CanTeamRespawn(uint8 TeamId) const
{
	return !TeamStates.IsValidIndex(TeamId) || !TeamStates[TeamId].bRespawnDisabled;
}

void ASavaGameState::InitTeams(int32 NumTeams)
{
	TeamStates.SetNum(NumTeams);
}

void ASavaGameState::SetRoundPhase(ESavaRoundPhase NewPhase, float Duration)
{
	RoundPhase = NewPhase;
	PhaseEndServerTime = Duration > 0.0f ? GetServerWorldTimeSeconds() + Duration : 0.0f;
}

void ASavaGameState::BeginNewRound()
{
	++RoundNumber;
	LastRoundWinner = NoTeam;
	for (FSavaTeamRoundState& TeamState : TeamStates)
	{
		TeamState.bRespawnDisabled = false;
	}
}

void ASavaGameState::SetTeamRespawnDisabled(uint8 TeamId, bool bDisabled)
{
	if (TeamStates.IsValidIndex(TeamId))
	{
		TeamStates[TeamId].bRespawnDisabled = bDisabled;
	}
}

void ASavaGameState::AddRoundWin(uint8 TeamId)
{
	LastRoundWinner = TeamId;
	if (TeamStates.IsValidIndex(TeamId))
	{
		++TeamStates[TeamId].RoundWins;
	}
}

void ASavaGameState::DrawDebugText() const
{
	if (!GEngine)
	{
		return;
	}

	FString Text = FString::Printf(TEXT("[CTF] Round %d  %s  %.0fs"),
		RoundNumber, *StaticEnum<ESavaRoundPhase>()->GetNameStringByValue(static_cast<int64>(RoundPhase)), GetPhaseTimeRemaining());
	for (int32 TeamIndex = 0; TeamIndex < TeamStates.Num(); ++TeamIndex)
	{
		Text += FString::Printf(TEXT("\n  Team %d: wins %d%s"),
			TeamIndex, TeamStates[TeamIndex].RoundWins, TeamStates[TeamIndex].bRespawnDisabled ? TEXT("  [NO RESPAWN]") : TEXT(""));
	}
	for (TActorIterator<ASavaFlag> It(GetWorld()); It; ++It)
	{
		Text += FString::Printf(TEXT("\n  Flag %d: %s"),
			It->GetTeamId(), *StaticEnum<ESavaFlagState>()->GetNameStringByValue(static_cast<int64>(It->GetFlagState())));
	}
	if (RoundPhase == ESavaRoundPhase::RoundEnd)
	{
		Text += LastRoundWinner == NoTeam ? FString(TEXT("\n  Round draw")) : FString::Printf(TEXT("\n  Team %d wins the round"), LastRoundWinner);
	}
	if (RoundPhase == ESavaRoundPhase::MatchEnd)
	{
		Text += MatchWinner == NoTeam ? FString(TEXT("\n  Match draw")) : FString::Printf(TEXT("\n  Team %d wins the match"), MatchWinner);
	}

	//同じキーで上書きする(毎フレーム積み重ならないように)
	GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 0.0f, FColor::Yellow, Text);
}

void ASavaGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASavaGameState, RoundPhase);
	DOREPLIFETIME(ASavaGameState, PhaseEndServerTime);
	DOREPLIFETIME(ASavaGameState, RoundNumber);
	DOREPLIFETIME(ASavaGameState, TeamStates);
	DOREPLIFETIME(ASavaGameState, LastRoundWinner);
	DOREPLIFETIME(ASavaGameState, MatchWinner);
}

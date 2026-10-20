// Fill out your copyright notice in the Description page of Project Settings.

#include "CTF/SavaGameState.h"
#include "CTF/SavaFlag.h"
#include "Player/SavaPlayerState.h"
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


//--------------------------------デスマッチ

void ASavaGameState::SetDeathmatchRules(bool bTeamMatch, int32 KillLimit)
{
	if (HasAuthority())
	{
		bIsDeathmatch = true;
		bDeathmatchTeamMatch = bTeamMatch;
		DeathmatchKillLimit = KillLimit;
	}
}

void ASavaGameState::SetDeathmatchResults(const TArray<ASavaPlayerState*>& Winners, int32 WinningKills, uint8 WinnerTeamId)
{
	if (!HasAuthority())
	{
		return;
	}

	DeathmatchWinners.Reset();
	DeathmatchWinnerNames.Reset();
	for (ASavaPlayerState* Winner : Winners)
	{
		if (Winner)
		{
			DeathmatchWinners.Add(Winner);
			DeathmatchWinnerNames.Add(Winner->GetPlayerName());
		}
	}
	DeathmatchWinningKills = WinningKills;
	MatchWinner = WinnerTeamId;
}

int32 ASavaGameState::GetTeamKills(uint8 TeamId) const
{
	int32 Kills = 0;
	for (const APlayerState* PlayerState : PlayerArray)
	{
		const ASavaPlayerState* SavaPlayerState = Cast<ASavaPlayerState>(PlayerState);
		if (SavaPlayerState && SavaPlayerState->GetTeamId() == TeamId)
		{
			Kills += SavaPlayerState->GetKillCount();
		}
	}
	return Kills;
}

TArray<ASavaPlayerState*> ASavaGameState::GetPlayersByKills() const
{
	TArray<ASavaPlayerState*> Players;
	for (APlayerState* PlayerState : PlayerArray)
	{
		ASavaPlayerState* SavaPlayerState = Cast<ASavaPlayerState>(PlayerState);
		if (SavaPlayerState && !SavaPlayerState->IsOnlyASpectator())
		{
			Players.Add(SavaPlayerState);
		}
	}
	Players.StableSort([](const ASavaPlayerState& A, const ASavaPlayerState& B)
	{
		if (A.GetKillCount() != B.GetKillCount())
		{
			return A.GetKillCount() > B.GetKillCount();
		}
		return A.GetDeathCount() < B.GetDeathCount();
	});
	return Players;
}

TArray<ASavaPlayerState*> ASavaGameState::GetDeathmatchWinners() const
{
	TArray<ASavaPlayerState*> Winners;
	for (ASavaPlayerState* Winner : DeathmatchWinners)
	{
		if (Winner)
		{
			Winners.Add(Winner);
		}
	}
	return Winners;
}

bool ASavaGameState::IsDeathmatchDraw() const
{
	return bDeathmatchTeamMatch ? MatchWinner == NoTeam : DeathmatchWinners.Num() > 1;
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

	if (bIsDeathmatch)
	{
		DrawDeathmatchDebugText();
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

void ASavaGameState::DrawDeathmatchDebugText() const
{
	FString Text = FString::Printf(TEXT("[DM] %s  %s  %.0fs  goal %d kills"),
		bDeathmatchTeamMatch ? TEXT("Team") : TEXT("FFA"),
		*StaticEnum<ESavaRoundPhase>()->GetNameStringByValue(static_cast<int64>(RoundPhase)), GetPhaseTimeRemaining(), DeathmatchKillLimit);

	if (bDeathmatchTeamMatch)
	{
		for (int32 TeamIndex = 0; TeamIndex < TeamStates.Num(); ++TeamIndex)
		{
			Text += FString::Printf(TEXT("\n  Team %d: %d kills"), TeamIndex, GetTeamKills(static_cast<uint8>(TeamIndex)));
		}
	}
	for (const ASavaPlayerState* Player : GetPlayersByKills())
	{
		const FString Team = bDeathmatchTeamMatch ? FString::Printf(TEXT(" (team %d)"), Player->GetTeamId()) : FString();
		Text += FString::Printf(TEXT("\n  %s%s: %d / %d"), *Player->GetPlayerName(), *Team, Player->GetKillCount(), Player->GetDeathCount());
	}

	if (RoundPhase == ESavaRoundPhase::MatchEnd)
	{
		if (IsDeathmatchDraw())
		{
			Text += TEXT("\n  Draw");
		}
		else if (bDeathmatchTeamMatch)
		{
			Text += FString::Printf(TEXT("\n  Team %d wins"), MatchWinner);
		}
		else if (DeathmatchWinnerNames.Num() > 0)
		{
			Text += FString::Printf(TEXT("\n  %s wins"), *DeathmatchWinnerNames[0]);
		}
	}

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
	DOREPLIFETIME(ASavaGameState, DeathmatchWinnerNames);
	DOREPLIFETIME(ASavaGameState, DeathmatchWinningKills);
	DOREPLIFETIME(ASavaGameState, DeathmatchWinners);
	DOREPLIFETIME(ASavaGameState, bIsDeathmatch);
	DOREPLIFETIME(ASavaGameState, bDeathmatchTeamMatch);
	DOREPLIFETIME(ASavaGameState, DeathmatchKillLimit);
}

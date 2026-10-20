// Fill out your copyright notice in the Description page of Project Settings.

#include "Lobby/SavaLobbyGameState.h"
#include "Lobby/SavaLobbySettings.h"
#include "Player/SavaPlayerState.h"
#include "Net/UnrealNetwork.h"

void ASavaLobbyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASavaLobbyGameState, RoomSettings);
	DOREPLIFETIME(ASavaLobbyGameState, bMatchStarting);
	DOREPLIFETIME(ASavaLobbyGameState, NumTeams);
	DOREPLIFETIME(ASavaLobbyGameState, MaxPlayersPerTeam);
}

void ASavaLobbyGameState::AddPlayerState(APlayerState* PlayerState)
{
	Super::AddPlayerState(PlayerState);
	NotifyLobbyChanged();
}

void ASavaLobbyGameState::RemovePlayerState(APlayerState* PlayerState)
{
	Super::RemovePlayerState(PlayerState);
	NotifyLobbyChanged();
}

TArray<ASavaPlayerState*> ASavaLobbyGameState::GetLobbyPlayers() const
{
	TArray<ASavaPlayerState*> Players;
	for (APlayerState* PlayerState : PlayerArray)
	{
		if (ASavaPlayerState* SavaPlayerState = Cast<ASavaPlayerState>(PlayerState); SavaPlayerState && !SavaPlayerState->IsOnlyASpectator())
		{
			Players.Add(SavaPlayerState);
		}
	}
	return Players;
}

TArray<ASavaPlayerState*> ASavaLobbyGameState::GetTeamPlayers(uint8 TeamId) const
{
	TArray<ASavaPlayerState*> Players = GetLobbyPlayers();
	Players.RemoveAll([TeamId](const ASavaPlayerState* PlayerState) { return PlayerState->GetTeamId() != TeamId; });
	return Players;
}

bool ASavaLobbyGameState::IsTeamMatch() const
{
	return USavaLobbySettings::UsesTeams(RoomSettings);
}

bool ASavaLobbyGameState::CanStartMatch(FText& OutReason) const
{
	if (bMatchStarting)
	{
		OutReason = FText::FromString(TEXT("試合を始めています"));
		return false;
	}

	return GetDefault<USavaLobbySettings>()->CanStartMatch(RoomSettings, PlayerArray, OutReason);
}

int32 ASavaLobbyGameState::CountTeamPlayers(const TArray<APlayerState*>& Players, uint8 TeamId, const APlayerState* Exclude)
{
	int32 Count = 0;
	for (const APlayerState* PlayerState : Players)
	{
		const ASavaPlayerState* SavaPlayerState = Cast<ASavaPlayerState>(PlayerState);
		if (SavaPlayerState && SavaPlayerState != Exclude && SavaPlayerState->GetTeamId() == TeamId)
		{
			++Count;
		}
	}
	return Count;
}

void ASavaLobbyGameState::NotifyLobbyChanged()
{
	OnLobbyChanged.Broadcast();
}

void ASavaLobbyGameState::InitTeams(int32 InNumTeams, int32 InMaxPlayersPerTeam)
{
	NumTeams = InNumTeams;
	MaxPlayersPerTeam = InMaxPlayersPerTeam;
}

void ASavaLobbyGameState::SetRoomSettings(const FSavaRoomSettings& NewSettings)
{
	if (HasAuthority() && !(RoomSettings == NewSettings))
	{
		RoomSettings = NewSettings;
		NotifyLobbyChanged(); //サーバーでは OnRep が呼ばれないので自分で呼ぶ
	}
}

void ASavaLobbyGameState::SetMatchStarting(bool bStarting)
{
	if (HasAuthority() && bMatchStarting != bStarting)
	{
		bMatchStarting = bStarting;
		NotifyLobbyChanged();
	}
}

void ASavaLobbyGameState::OnRep_LobbyInfo()
{
	NotifyLobbyChanged();
}

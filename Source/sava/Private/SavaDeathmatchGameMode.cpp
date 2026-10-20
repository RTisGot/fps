// Fill out your copyright notice in the Description page of Project Settings.

#include "SavaDeathmatchGameMode.h"
#include "CTF/SavaGameState.h"
#include "Lobby/SavaLobbyTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Player/SavaPlayerController.h"
#include "Player/SavaPlayerState.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogSavaDeathmatch, Log, All);

ASavaDeathmatchGameMode::ASavaDeathmatchGameMode()
{
	GameStateClass = ASavaGameState::StaticClass();

	//既定は個人戦(全員チームなし)。チーム戦はルーム設定の Teams で選ぶ
	bUseTeams = false;
}

ASavaGameState* ASavaDeathmatchGameMode::GetSavaGameState() const
{
	return GetGameState<ASavaGameState>();
}

void ASavaDeathmatchGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	//ロビーのルーム設定(付いていなければ、この GameMode に設定した値のまま)
	MatchDuration = FMath::Max(1.0f, static_cast<float>(UGameplayStatics::GetIntOption(Options, SavaMatchOptions::TimeLimit, FMath::RoundToInt(MatchDuration))));
	KillLimit = FMath::Max(1, UGameplayStatics::GetIntOption(Options, SavaMatchOptions::KillLimit, KillLimit));
	bUseTeams = UGameplayStatics::GetIntOption(Options, SavaMatchOptions::TeamMatch, bUseTeams ? 1 : 0) != 0;
	ExpectedPlayers = FMath::Max(0, UGameplayStatics::GetIntOption(Options, SavaMatchOptions::ExpectedPlayers, 0));

	//GameState は InitGame より先に作られているので、チーム数はここで入れる
	if (ASavaGameState* SavaGameState = GetSavaGameState())
	{
		SavaGameState->InitTeams(bUseTeams ? NumTeams : 0);
	}

	UE_LOG(LogSavaDeathmatch, Log, TEXT("Match settings: %s, %.0f s, %d kills to win, waiting for %d players."),
		bUseTeams ? TEXT("team") : TEXT("free-for-all"), MatchDuration, KillLimit, ExpectedPlayers);
}

int32 ASavaDeathmatchGameMode::GetTeamKills(uint8 TeamId) const
{
	int32 Kills = 0;
	if (const ASavaGameState* SavaGameState = GetSavaGameState())
	{
		for (const APlayerState* PlayerState : SavaGameState->PlayerArray)
		{
			const ASavaPlayerState* SavaPlayerState = Cast<ASavaPlayerState>(PlayerState);
			if (SavaPlayerState && SavaPlayerState->GetTeamId() == TeamId)
			{
				Kills += SavaPlayerState->GetKillCount();
			}
		}
	}
	return Kills;
}

TArray<FString> ASavaDeathmatchGameMode::GetTeamPlayerNames(uint8 TeamId) const
{
	TArray<FString> Names;
	if (const ASavaGameState* SavaGameState = GetSavaGameState())
	{
		for (const APlayerState* PlayerState : SavaGameState->PlayerArray)
		{
			const ASavaPlayerState* SavaPlayerState = Cast<ASavaPlayerState>(PlayerState);
			if (SavaPlayerState && SavaPlayerState->GetTeamId() == TeamId)
			{
				Names.Add(SavaPlayerState->GetPlayerName());
			}
		}
	}
	return Names;
}

//--------------------------------試合の開始

void ASavaDeathmatchGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	NotifyPlayerArrived(NewPlayer);
}

void ASavaDeathmatchGameMode::HandleSeamlessTravelPlayer(AController*& C)
{
	Super::HandleSeamlessTravelPlayer(C); //この中で PlayerController が作り直され、C が新しいものに変わる
	NotifyPlayerArrived(C);
}

void ASavaDeathmatchGameMode::Logout(AController* Exiting)
{
	ArrivedPlayers.Remove(Exiting);
	Super::Logout(Exiting);
}

void ASavaDeathmatchGameMode::NotifyPlayerArrived(AController* Controller)
{
	if (Controller)
	{
		ArrivedPlayers.Add(Controller);
	}

	//待合室から来たなら、全員そろわなくても一定時間で始める
	if (ExpectedPlayers > 0 && !GetWorldTimerManager().IsTimerActive(WaitForPlayersTimer))
	{
		GetWorldTimerManager().SetTimer(WaitForPlayersTimer, this, &ASavaDeathmatchGameMode::OnWaitForPlayersTimeout, FMath::Max(WaitForPlayersTimeout, 0.01f), false);
	}

	TryStartMatch(false);
}

void ASavaDeathmatchGameMode::OnWaitForPlayersTimeout()
{
	TryStartMatch(true);
}

void ASavaDeathmatchGameMode::TryStartMatch(bool bTimedOut)
{
	const ASavaGameState* SavaGameState = GetSavaGameState();
	if (!SavaGameState || SavaGameState->GetRoundPhase() != ESavaRoundPhase::WaitingToStart || GetWorldTimerManager().IsTimerActive(PhaseTimer))
	{
		return;
	}

	//抜けた人は数えない
	for (auto It = ArrivedPlayers.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
		{
			It.RemoveCurrent();
		}
	}

	//待合室から来たなら全員(時間切れなら MinPlayersToStart 人)、直接開いたなら MinPlayersToStart 人そろったら始める
	const int32 RequiredPlayers = bTimedOut ? MinPlayersToStart : FMath::Max(MinPlayersToStart, ExpectedPlayers);
	if (ArrivedPlayers.Num() < RequiredPlayers)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(WaitForPlayersTimer);
	//体のスポーンが終わった次のフレームで始める
	PhaseTimer = GetWorldTimerManager().SetTimerForNextTick(this, &ASavaDeathmatchGameMode::StartCountdown);
}

void ASavaDeathmatchGameMode::StartCountdown()
{
	ASavaGameState* SavaGameState = GetSavaGameState();
	if (!SavaGameState)
	{
		return;
	}

	//待っている間のキル・デスは数えない
	for (APlayerState* PlayerState : SavaGameState->PlayerArray)
	{
		if (ASavaPlayerState* SavaPlayerState = Cast<ASavaPlayerState>(PlayerState))
		{
			SavaPlayerState->ResetKillsAndDeaths();
		}
	}

	SavaGameState->SetRoundPhase(ESavaRoundPhase::PreRound, PreMatchDuration);
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		SetRoundFrozen(It->Get(), true);
	}
	UE_LOG(LogSavaDeathmatch, Log, TEXT("Countdown."));

	if (PreMatchDuration > 0.0f)
	{
		GetWorldTimerManager().SetTimer(PhaseTimer, this, &ASavaDeathmatchGameMode::BeginMatchPlay, PreMatchDuration, false);
	}
	else
	{
		BeginMatchPlay();
	}
}

void ASavaDeathmatchGameMode::BeginMatchPlay()
{
	ASavaGameState* SavaGameState = GetSavaGameState();
	if (!SavaGameState)
	{
		return;
	}

	SavaGameState->SetRoundPhase(ESavaRoundPhase::InRound, MatchDuration);
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		SetRoundFrozen(It->Get(), false);
	}
	GetWorldTimerManager().SetTimer(PhaseTimer, this, &ASavaDeathmatchGameMode::OnMatchTimeUp, MatchDuration, false);
	UE_LOG(LogSavaDeathmatch, Log, TEXT("Start."));
}

//--------------------------------スポーン・ロードアウト

void ASavaDeathmatchGameMode::RestartPlayer(AController* NewPlayer)
{
	Super::RestartPlayer(NewPlayer);

	//カウントダウン中に出た人は、開始まで動けない
	const ASavaGameState* SavaGameState = GetSavaGameState();
	if (SavaGameState && SavaGameState->GetRoundPhase() == ESavaRoundPhase::PreRound)
	{
		SetRoundFrozen(NewPlayer, true);
	}
}

void ASavaDeathmatchGameMode::RespawnPlayer(TWeakObjectPtr<AController> Controller)
{
	//試合が終わったら、リスポーン待ちの人も出さない
	const ASavaGameState* SavaGameState = GetSavaGameState();
	if (SavaGameState && SavaGameState->GetRoundPhase() == ESavaRoundPhase::MatchEnd)
	{
		return;
	}
	Super::RespawnPlayer(Controller);
}

bool ASavaDeathmatchGameMode::CanChangeLoadout(const ASavaPlayerState* PlayerState) const
{
	return PlayerState && !PlayerState->HasReceivedLoadout();
}

//--------------------------------試合の終了

void ASavaDeathmatchGameMode::CheckKillLimit(ASavaPlayerState* Killer)
{
	const ASavaGameState* SavaGameState = GetSavaGameState();
	if (!Killer || !SavaGameState || SavaGameState->GetRoundPhase() != ESavaRoundPhase::InRound)
	{
		return;
	}

	if (bUseTeams)
	{
		const uint8 TeamId = Killer->GetTeamId();
		const int32 TeamKills = GetTeamKills(TeamId);
		if (TeamKills >= KillLimit)
		{
			UE_LOG(LogSavaDeathmatch, Log, TEXT("Kill limit reached: team %d"), TeamId);
			FinishMatch(GetTeamPlayerNames(TeamId), TeamKills, TeamId);
		}
		return;
	}

	if (Killer->GetKillCount() >= KillLimit)
	{
		UE_LOG(LogSavaDeathmatch, Log, TEXT("Kill limit reached: %s"), *Killer->GetPlayerName());
		FinishMatch({ Killer->GetPlayerName() }, Killer->GetKillCount(), ASavaGameState::NoTeam);
	}
}

void ASavaDeathmatchGameMode::OnMatchTimeUp()
{
	const ASavaGameState* SavaGameState = GetSavaGameState();
	if (!SavaGameState || SavaGameState->GetRoundPhase() != ESavaRoundPhase::InRound)
	{
		return;
	}

	//チーム戦: 合計キルが一番多いチーム(同数なら引き分けで、並んだチームの全員)
	if (bUseTeams)
	{
		TArray<uint8> WinnerTeams;
		int32 WinningTeamKills = 0;
		for (int32 TeamIndex = 0; TeamIndex < NumTeams; ++TeamIndex)
		{
			const int32 TeamKills = GetTeamKills(static_cast<uint8>(TeamIndex));
			if (WinnerTeams.IsEmpty() || TeamKills > WinningTeamKills)
			{
				WinningTeamKills = TeamKills;
				WinnerTeams.Reset();
				WinnerTeams.Add(static_cast<uint8>(TeamIndex));
			}
			else if (TeamKills == WinningTeamKills)
			{
				WinnerTeams.Add(static_cast<uint8>(TeamIndex));
			}
		}

		TArray<FString> TeamWinnerNames;
		for (const uint8 TeamId : WinnerTeams)
		{
			TeamWinnerNames.Append(GetTeamPlayerNames(TeamId));
		}
		UE_LOG(LogSavaDeathmatch, Log, TEXT("Time up. %d team(s) tied at the top with %d kills."), WinnerTeams.Num(), WinningTeamKills);
		FinishMatch(TeamWinnerNames, WinningTeamKills, WinnerTeams.Num() == 1 ? WinnerTeams[0] : ASavaGameState::NoTeam);
		return;
	}

	//個人戦: キル数が一番多い人(同数なら全員)
	TArray<FString> WinnerNames;
	int32 WinningKills = 0;
	for (APlayerState* PlayerState : SavaGameState->PlayerArray)
	{
		const ASavaPlayerState* SavaPlayerState = Cast<ASavaPlayerState>(PlayerState);
		if (!SavaPlayerState || SavaPlayerState->IsOnlyASpectator())
		{
			continue;
		}

		const int32 Kills = SavaPlayerState->GetKillCount();
		if (WinnerNames.IsEmpty() || Kills > WinningKills)
		{
			WinningKills = Kills;
			WinnerNames.Reset();
			WinnerNames.Add(SavaPlayerState->GetPlayerName());
		}
		else if (Kills == WinningKills)
		{
			WinnerNames.Add(SavaPlayerState->GetPlayerName());
		}
	}

	UE_LOG(LogSavaDeathmatch, Log, TEXT("Time up. %d winner(s) with %d kills."), WinnerNames.Num(), WinningKills);
	FinishMatch(WinnerNames, WinningKills, ASavaGameState::NoTeam);
}

void ASavaDeathmatchGameMode::FinishMatch(const TArray<FString>& WinnerNames, int32 WinningKills, uint8 WinnerTeamId)
{

	UE_LOG(LogTemp, Warning, TEXT("FinishMatch called"));
	ASavaGameState* SavaGameState = GetSavaGameState();
	if (!SavaGameState || SavaGameState->GetRoundPhase() == ESavaRoundPhase::MatchEnd)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(PhaseTimer);

	SavaGameState->SetDeathmatchResults(WinnerNames, WinningKills);
	SavaGameState->SetMatchWinner(WinnerTeamId);
	//残り時間 = ロビーへ戻るまでの秒数(画面にカウントダウンを出せる)
	SavaGameState->SetRoundPhase(ESavaRoundPhase::MatchEnd, ReturnToLobbyDelay);

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		SetRoundFrozen(It->Get(), true);
		if (ASavaPlayerController* SavaPlayerController = Cast<ASavaPlayerController>(It->Get()))
		{
			SavaPlayerController->ClientShowDeathmatchResult();
		}
	}

	UE_LOG(LogSavaDeathmatch, Log, TEXT("Match end. Returning to the lobby in %.0f s."), ReturnToLobbyDelay);
	ScheduleReturnToLobby();
}

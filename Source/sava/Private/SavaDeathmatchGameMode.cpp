// Fill out your copyright notice in the Description page of Project Settings.

#include "SavaDeathmatchGameMode.h"
#include "CTF/SavaGameState.h"
#include "CTF/SavaTeamPlayerStart.h"
#include "Lobby/SavaLobbyTypes.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Player/SavaPlayerController.h"
#include "Player/SavaPlayerState.h"
#include "TimerManager.h"
#include "savaCharacter.h"

DEFINE_LOG_CATEGORY_STATIC(LogSavaDeathmatch, Log, All);

namespace
{
	//この距離より近くに生きている人がいるスポーン地点は使用中とみなす
	constexpr float OccupiedStartRadius = 150.0f;

	//一番安全な地点の何割まで離れていれば候補にするか(毎回同じ地点にならないように少し幅を持たせる)
	constexpr float SafeStartRatio = 0.8f;
}

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

	UE_LOG(LogSavaDeathmatch, Log, TEXT("Match settings: %s, %.0f s, %d kills to win, waiting for %d players."),
		bUseTeams ? TEXT("team") : TEXT("free-for-all"), MatchDuration, KillLimit, ExpectedPlayers);
}

void ASavaDeathmatchGameMode::InitGameState()
{
	Super::InitGameState();

	//GameState は InitGame の後に作られるので、試合のルールはここで入れる(UI・デバッグ表示が読む)
	if (ASavaGameState* SavaGameState = GetSavaGameState())
	{
		SavaGameState->InitTeams(bUseTeams ? NumTeams : 0);
		SavaGameState->SetDeathmatchRules(bUseTeams, KillLimit);
	}
}

TArray<ASavaPlayerState*> ASavaDeathmatchGameMode::GetTeamPlayers(uint8 TeamId) const
{
	TArray<ASavaPlayerState*> Players;
	if (const ASavaGameState* SavaGameState = GetSavaGameState())
	{
		for (APlayerState* PlayerState : SavaGameState->PlayerArray)
		{
			ASavaPlayerState* SavaPlayerState = Cast<ASavaPlayerState>(PlayerState);
			if (SavaPlayerState && SavaPlayerState->GetTeamId() == TeamId)
			{
				Players.Add(SavaPlayerState);
			}
		}
	}
	return Players;
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

bool ASavaDeathmatchGameMode::ShouldSpawnAtStartSpot(AController* Player)
{
	//毎回 ChoosePlayerStart で選び直す(既定では最初に出た地点を使い続ける)
	return false;
}

AActor* ASavaDeathmatchGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	const ASavaPlayerState* SavaPlayerState = Player ? Player->GetPlayerState<ASavaPlayerState>() : nullptr;
	const uint8 MyTeam = SavaPlayerState ? SavaPlayerState->GetTeamId() : ASavaPlayerState::NoTeam;

	//候補: チーム戦なら自チームの地点(なければ全部)、個人戦ならチーム用も含めた全部
	TArray<APlayerStart*> AllStarts;
	TArray<APlayerStart*> TeamStarts;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		AllStarts.Add(*It);
		const ASavaTeamPlayerStart* TeamStart = Cast<ASavaTeamPlayerStart>(*It);
		if (bUseTeams && TeamStart && TeamStart->GetTeamId() == MyTeam)
		{
			TeamStarts.Add(*It);
		}
	}
	const TArray<APlayerStart*>& Starts = TeamStarts.Num() > 0 ? TeamStarts : AllStarts;
	if (Starts.Num() == 0)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	//生きている人の位置(自分の体は除く)。個人戦は全員が敵、チーム戦は他チームの人が敵
	TArray<FVector> EnemyLocations;
	TArray<FVector> OccupiedLocations;
	const APawn* MyPawn = Player ? Player->GetPawn() : nullptr;
	for (TActorIterator<AsavaCharacter> It(GetWorld()); It; ++It)
	{
		if (!IsValid(*It) || It->IsDead() || *It == MyPawn)
		{
			continue;
		}
		OccupiedLocations.Add(It->GetActorLocation());

		const ASavaPlayerState* OtherPlayerState = It->GetPlayerState<ASavaPlayerState>();
		if (OtherPlayerState == SavaPlayerState)
		{
			continue;
		}
		if (!bUseTeams || !OtherPlayerState || OtherPlayerState->GetTeamId() != MyTeam)
		{
			EnemyLocations.Add(It->GetActorLocation());
		}
	}

	TArray<FVector> StartLocations;
	for (const APlayerStart* Start : Starts)
	{
		StartLocations.Add(Start->GetActorLocation());
	}

	const TArray<int32> Candidates = FindSafestStarts(StartLocations, EnemyLocations, OccupiedLocations);
	return Starts[Candidates[FMath::RandRange(0, Candidates.Num() - 1)]];
}

TArray<int32> ASavaDeathmatchGameMode::FindSafestStarts(const TArray<FVector>& StartLocations, const TArray<FVector>& EnemyLocations, const TArray<FVector>& OccupiedLocations)
{
	//使用中でない地点
	TArray<int32> FreeStarts;
	for (int32 Index = 0; Index < StartLocations.Num(); ++Index)
	{
		const bool bOccupied = OccupiedLocations.ContainsByPredicate([&](const FVector& Location)
		{
			return FVector::DistSquared(Location, StartLocations[Index]) < FMath::Square(OccupiedStartRadius);
		});
		if (!bOccupied)
		{
			FreeStarts.Add(Index);
		}
	}
	if (FreeStarts.Num() == 0)
	{
		for (int32 Index = 0; Index < StartLocations.Num(); ++Index)
		{
			FreeStarts.Add(Index);
		}
	}
	if (EnemyLocations.Num() == 0)
	{
		return FreeStarts;
	}

	//一番近い敵までの距離
	TArray<float> Safety;
	float BestSafety = 0.0f;
	for (const int32 Index : FreeStarts)
	{
		float Nearest = TNumericLimits<float>::Max();
		for (const FVector& Enemy : EnemyLocations)
		{
			Nearest = FMath::Min(Nearest, FVector::Dist(Enemy, StartLocations[Index]));
		}
		Safety.Add(Nearest);
		BestSafety = FMath::Max(BestSafety, Nearest);
	}

	TArray<int32> SafeStarts;
	for (int32 Candidate = 0; Candidate < FreeStarts.Num(); ++Candidate)
	{
		if (Safety[Candidate] >= BestSafety * SafeStartRatio)
		{
			SafeStarts.Add(FreeStarts[Candidate]);
		}
	}
	return SafeStarts;
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
		const int32 TeamKills = SavaGameState->GetTeamKills(TeamId);
		if (TeamKills >= KillLimit)
		{
			UE_LOG(LogSavaDeathmatch, Log, TEXT("Kill limit reached: team %d"), TeamId);
			FinishMatch(GetTeamPlayers(TeamId), TeamKills, TeamId);
		}
		return;
	}

	if (Killer->GetKillCount() >= KillLimit)
	{
		UE_LOG(LogSavaDeathmatch, Log, TEXT("Kill limit reached: %s"), *Killer->GetPlayerName());
		FinishMatch({ Killer }, Killer->GetKillCount(), ASavaGameState::NoTeam);
	}
}

void ASavaDeathmatchGameMode::OnMatchTimeUp()
{
	const ASavaGameState* SavaGameState = GetSavaGameState();
	if (!SavaGameState || SavaGameState->GetRoundPhase() != ESavaRoundPhase::InRound)
	{
		return;
	}

	TArray<ASavaPlayerState*> Winners;
	int32 WinningKills = 0;
	uint8 WinnerTeamId = ASavaGameState::NoTeam;
	DecideWinners(SavaGameState->PlayerArray, bUseTeams, NumTeams, Winners, WinningKills, WinnerTeamId);

	UE_LOG(LogSavaDeathmatch, Log, TEXT("Time up. %d winner(s) with %d kills (team %d)."), Winners.Num(), WinningKills, WinnerTeamId);
	FinishMatch(Winners, WinningKills, WinnerTeamId);
}

void ASavaDeathmatchGameMode::DecideWinners(const TArray<APlayerState*>& Players, bool bTeamMatch, int32 NumTeams,
	TArray<ASavaPlayerState*>& OutWinners, int32& OutWinningKills, uint8& OutWinnerTeamId)
{
	OutWinners.Reset();
	OutWinningKills = 0;
	OutWinnerTeamId = ASavaGameState::NoTeam;

	TArray<ASavaPlayerState*> SavaPlayers;
	for (APlayerState* PlayerState : Players)
	{
		ASavaPlayerState* SavaPlayerState = Cast<ASavaPlayerState>(PlayerState);
		if (SavaPlayerState && !SavaPlayerState->IsOnlyASpectator())
		{
			SavaPlayers.Add(SavaPlayerState);
		}
	}

	//個人戦: キル数が一番多い人(同数なら全員)
	if (!bTeamMatch)
	{
		for (ASavaPlayerState* Player : SavaPlayers)
		{
			const int32 Kills = Player->GetKillCount();
			if (OutWinners.IsEmpty() || Kills > OutWinningKills)
			{
				OutWinningKills = Kills;
				OutWinners.Reset();
				OutWinners.Add(Player);
			}
			else if (Kills == OutWinningKills)
			{
				OutWinners.Add(Player);
			}
		}
		return;
	}

	//チーム戦: 合計キルが一番多いチーム(同数なら引き分けで、並んだチームの全員)
	TArray<uint8> WinnerTeams;
	for (int32 TeamIndex = 0; TeamIndex < NumTeams; ++TeamIndex)
	{
		int32 TeamKills = 0;
		for (const ASavaPlayerState* Player : SavaPlayers)
		{
			if (Player->GetTeamId() == TeamIndex)
			{
				TeamKills += Player->GetKillCount();
			}
		}

		if (WinnerTeams.IsEmpty() || TeamKills > OutWinningKills)
		{
			OutWinningKills = TeamKills;
			WinnerTeams.Reset();
			WinnerTeams.Add(static_cast<uint8>(TeamIndex));
		}
		else if (TeamKills == OutWinningKills)
		{
			WinnerTeams.Add(static_cast<uint8>(TeamIndex));
		}
	}

	for (ASavaPlayerState* Player : SavaPlayers)
	{
		if (WinnerTeams.Contains(Player->GetTeamId()))
		{
			OutWinners.Add(Player);
		}
	}
	OutWinnerTeamId = WinnerTeams.Num() == 1 ? WinnerTeams[0] : ASavaGameState::NoTeam;
}

void ASavaDeathmatchGameMode::FinishMatch(const TArray<ASavaPlayerState*>& Winners, int32 WinningKills, uint8 WinnerTeamId)
{

	UE_LOG(LogTemp, Warning, TEXT("FinishMatch called"));
	ASavaGameState* SavaGameState = GetSavaGameState();
	if (!SavaGameState || SavaGameState->GetRoundPhase() == ESavaRoundPhase::MatchEnd)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(PhaseTimer);

	SavaGameState->SetDeathmatchResults(Winners, WinningKills, WinnerTeamId);
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

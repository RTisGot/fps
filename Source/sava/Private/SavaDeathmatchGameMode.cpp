// Fill out your copyright notice in the Description page of Project Settings.


#include "SavaDeathmatchGameMode.h"
#include "CTF/SavaGameState.h"
#include "TimerManager.h"
#include "Player/SavaPlayerState.h"

ASavaDeathmatchGameMode::ASavaDeathmatchGameMode()
{
    GameStateClass = ASavaGameState::StaticClass();
}

void ASavaDeathmatchGameMode::BeginPlay()
{
    Super::BeginPlay();

    if (!HasAuthority())
    {
        return;
    }

    ASavaGameState* SavaGameState = GetGameState<ASavaGameState>();
    if (!SavaGameState)
    {
        UE_LOG(LogTemp, Error,
            TEXT("SavaDeathmatchGameMode: GameState is invalid."));
        return;
    }

    // Šù‘¶‚ÌŽžŠÔŠÇ—‚ð—˜—p‚·‚é
    SavaGameState->SetRoundPhase(
        ESavaRoundPhase::InRound,
        MatchDuration
    );

    GetWorldTimerManager().SetTimer(
        MatchTimerHandle,
        this,
        &ASavaDeathmatchGameMode::UpdateMatchTime,
        1.0f,
        true
    );
}

void ASavaDeathmatchGameMode::UpdateMatchTime()
{
    const ASavaGameState* SavaGameState =
        GetGameState<ASavaGameState>();

    if (!SavaGameState)
    {
        return;
    }

    if (SavaGameState->GetPhaseTimeRemaining() <= 0.0f)
    {
        FinishMatch();
    }
}


void ASavaDeathmatchGameMode::FinishMatch()
{
    if (!HasAuthority())
    {
        return;
    }

    GetWorldTimerManager().ClearTimer(MatchTimerHandle);

    ASavaGameState* SavaGameState =
        GetGameState<ASavaGameState>();

    if (!SavaGameState)
    {
        UE_LOG(LogTemp, Error,
            TEXT("Deathmatch: GameState is invalid."));
        return;
    }

    TArray<FString> WinnerNames;
    int32 WinningKills = -1;

    for (APlayerState* PlayerState :
        SavaGameState->PlayerArray)
    {
        ASavaPlayerState* SavaPlayerState =
            Cast<ASavaPlayerState>(PlayerState);

        if (!SavaPlayerState)
        {
            continue;
        }

        const int32 Kills =
            SavaPlayerState->GetKillCount();

        UE_LOG(LogTemp, Warning,
            TEXT("[Deathmatch] Player=%s, Kills=%d"),
            *SavaPlayerState->GetPlayerName(),
            Kills);

        if (Kills > WinningKills)
        {
            WinningKills = Kills;
            WinnerNames.Empty();
            WinnerNames.Add(
                SavaPlayerState->GetPlayerName());
        }
        else if (Kills == WinningKills)
        {
            WinnerNames.Add(
                SavaPlayerState->GetPlayerName());
        }
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[Deathmatch] WinnerCount=%d, WinningKills=%d"),
        WinnerNames.Num(),
        WinningKills);

    if (WinnerNames.IsEmpty())
    {
        WinningKills = 0;
        UE_LOG(LogTemp, Warning,
            TEXT("Deathmatch: No players found."));
    }
    else if (WinnerNames.Num() == 1)
    {
        UE_LOG(LogTemp, Log,
            TEXT("Winner: %s, Kills: %d"),
            *WinnerNames[0], WinningKills);
    }
    else
    {
        UE_LOG(LogTemp, Log,
            TEXT("Draw: %d players tied at %d kills."),
            WinnerNames.Num(), WinningKills);
    }

    SavaGameState->SetDeathmatchResults(
        WinnerNames, WinningKills);

    SavaGameState->SetRoundPhase(
        ESavaRoundPhase::MatchEnd, 0.0f);
}

void ASavaDeathmatchGameMode::FinishMatchWithWinner(
    ASavaPlayerState* Winner)
{
    if (!HasAuthority() || !Winner)
    {
        return;
    }

    ASavaGameState* SavaGameState =
        GetGameState<ASavaGameState>();

    if (!SavaGameState ||
        SavaGameState->GetRoundPhase() ==
        ESavaRoundPhase::MatchEnd)
    {
        return;
    }

    GetWorldTimerManager().ClearTimer(MatchTimerHandle);

    TArray<FString> WinnerNames;
    WinnerNames.Add(Winner->GetPlayerName());

    const int32 WinningKills = Winner->GetKillCount();

    SavaGameState->SetDeathmatchResults(
        WinnerNames, WinningKills);

    SavaGameState->SetRoundPhase(
        ESavaRoundPhase::MatchEnd, 0.0f);

    UE_LOG(LogTemp, Warning,
        TEXT("Kill limit winner: %s / Kills: %d"),
        *Winner->GetPlayerName(), WinningKills);
}

void ASavaDeathmatchGameMode::CheckKillLimit(
    ASavaPlayerState* Killer)
{
    if (!HasAuthority() || !Killer)
    {
        return;
    }

    ASavaGameState* SavaGameState =
        GetGameState<ASavaGameState>();

    if (!SavaGameState ||
        SavaGameState->GetRoundPhase() ==
        ESavaRoundPhase::MatchEnd)
    {
        return;
    }

    if (Killer->GetKillCount() >= KillLimit)
    {
        FinishMatchWithWinner(Killer);
    }
}
// Fill out your copyright notice in the Description page of Project Settings.

#include "Lobby/SavaLobbySettings.h"
#include "Player/SavaPlayerState.h"
#include "GameFramework/GameModeBase.h"
#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING
//少ない人数で開始を試すため(ホストの PC で設定する)。0 ならモードの MinPlayers のまま
static TAutoConsoleVariable<int32> CVarSavaLobbyMinPlayersOverride(
	TEXT("Sava.Lobby.MinPlayersOverride"), 0,
	TEXT("開発用: 試合の開始に必要な人数を上書きする(0 で無効)。例: Sava.Lobby.MinPlayersOverride 2"));
#endif

FSavaRoomSettings USavaLobbySettings::MakeDefaultRoomSettings() const
{
	FSavaRoomSettings RoomSettings;
	RoomSettings.ModeIndex = 0;
	RoomSettings.MapIndex = 0;
	RoomSettings.RoundsToWin = DefaultRoundsToWin;
	RoomSettings.RoundTimeLimit = DefaultRoundTimeLimit;
	RoomSettings.KillLimit = DefaultKillLimit;
	return Sanitize(RoomSettings);
}

FSavaRoomSettings USavaLobbySettings::Sanitize(const FSavaRoomSettings& InSettings) const
{
	FSavaRoomSettings Result = InSettings;

	Result.ModeIndex = Modes.IsValidIndex(Result.ModeIndex) ? Result.ModeIndex : 0;
	const FSavaMatchModeOption* Mode = FindMode(Result.ModeIndex);
	Result.MapIndex = Mode && Mode->Maps.IsValidIndex(Result.MapIndex) ? Result.MapIndex : 0;

	Result.RoundsToWin = FMath::Clamp(Result.RoundsToWin, MinRoundsToWin, FMath::Max(MinRoundsToWin, MaxRoundsToWin));
	Result.RoundTimeLimit = FMath::Clamp(Result.RoundTimeLimit, MinRoundTimeLimit, FMath::Max(MinRoundTimeLimit, MaxRoundTimeLimit));
	Result.KillLimit = FMath::Clamp(Result.KillLimit, MinKillLimit, FMath::Max(MinKillLimit, MaxKillLimit));
	return Result;
}

const FSavaMatchModeOption* USavaLobbySettings::FindMode(int32 ModeIndex) const
{
	return Modes.IsValidIndex(ModeIndex) ? &Modes[ModeIndex] : nullptr;
}

bool USavaLobbySettings::CanStartMatch(const FSavaRoomSettings& RoomSettings, const TArray<APlayerState*>& Players, FText& OutReason) const
{
	const FSavaMatchModeOption* Mode = FindMode(RoomSettings.ModeIndex);
	if (!Mode)
	{
		OutReason = FText::FromString(TEXT("モードが選ばれていません"));
		return false;
	}
	if (Mode->GameModeClass.IsNull())
	{
		OutReason = FText::FromString(TEXT("このモードはまだ遊べません"));
		return false;
	}
	if (!Mode->Maps.IsValidIndex(RoomSettings.MapIndex) || Mode->Maps[RoomSettings.MapIndex].Map.IsNull())
	{
		OutReason = FText::FromString(TEXT("マップが選ばれていません"));
		return false;
	}

	int32 NumPlayers = 0;
	int32 NumNotReady = 0;
	for (const APlayerState* PlayerState : Players)
	{
		const ASavaPlayerState* SavaPlayerState = Cast<ASavaPlayerState>(PlayerState);
		if (!SavaPlayerState || SavaPlayerState->IsOnlyASpectator())
		{
			continue;
		}

		++NumPlayers;
		//ホストは準備 OK を押さなくてよい(開始ボタンを押すのがホストのため)
		if (!SavaPlayerState->IsRoomHost() && !SavaPlayerState->IsReady())
		{
			++NumNotReady;
		}
	}

	int32 MinPlayers = Mode->MinPlayers;
#if !UE_BUILD_SHIPPING
	if (CVarSavaLobbyMinPlayersOverride.GetValueOnAnyThread() > 0)
	{
		MinPlayers = CVarSavaLobbyMinPlayersOverride.GetValueOnAnyThread();
	}
#endif
	if (NumPlayers < MinPlayers)
	{
		OutReason = FText::FromString(FString::Printf(TEXT("%d 人以上必要です(今 %d 人)"), MinPlayers, NumPlayers));
		return false;
	}
	if (NumNotReady > 0)
	{
		OutReason = FText::FromString(FString::Printf(TEXT("準備 OK でない人が %d 人います"), NumNotReady));
		return false;
	}

	OutReason = FText::GetEmpty();
	return true;
}

TArray<FSavaMatchModeOption> USavaLobbySettings::GetModeOptions()
{
	return GetDefault<USavaLobbySettings>()->Modes;
}

bool USavaLobbySettings::GetModeOption(int32 ModeIndex, FSavaMatchModeOption& OutMode)
{
	if (const FSavaMatchModeOption* Mode = GetDefault<USavaLobbySettings>()->FindMode(ModeIndex))
	{
		OutMode = *Mode;
		return true;
	}
	return false;
}

void USavaLobbySettings::GetRoomSettingsDisplayNames(const FSavaRoomSettings& RoomSettings, FText& OutModeName, FText& OutMapName)
{
	OutModeName = FText::GetEmpty();
	OutMapName = FText::GetEmpty();

	if (const FSavaMatchModeOption* Mode = GetDefault<USavaLobbySettings>()->FindMode(RoomSettings.ModeIndex))
	{
		OutModeName = Mode->DisplayName;
		if (Mode->Maps.IsValidIndex(RoomSettings.MapIndex))
		{
			OutMapName = Mode->Maps[RoomSettings.MapIndex].DisplayName;
		}
	}
}

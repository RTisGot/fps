// Fill out your copyright notice in the Description page of Project Settings.

#include "Lobby/SavaLobbyGameMode.h"
#include "Lobby/SavaLobbyGameState.h"
#include "Lobby/SavaLobbySettings.h"
#include "Online/SavaSessionSubsystem.h"
#include "Player/SavaPlayerController.h"
#include "Player/SavaPlayerState.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameSession.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogSavaLobby, Log, All);

ASavaLobbyGameMode::ASavaLobbyGameMode()
{
	GameStateClass = ASavaLobbyGameState::StaticClass();

	//試合マップへは接続を切らずに全員を連れていく(bUseSeamlessTravel は AsavaGameMode で true)
	//PIE では net.AllowPIESeamlessTravel=1 のときだけ効く(DefaultEngine.ini で設定済み)
}

ASavaLobbyGameState* ASavaLobbyGameMode::GetLobbyGameState() const
{
	return GetGameState<ASavaLobbyGameState>();
}

void ASavaLobbyGameMode::InitGameState()
{
	Super::InitGameState();

	if (ASavaLobbyGameState* LobbyGameState = GetLobbyGameState())
	{
		LobbyGameState->InitTeams(NumTeams, MaxPlayersPerTeam);

		//試合から戻ってきたなら、前の試合のルーム設定のまま(部屋を作った直後は最初の設定)
		const USavaLobbySettings* LobbySettings = GetDefault<USavaLobbySettings>();
		const USavaSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<USavaSessionSubsystem>();
		const TOptional<FSavaRoomSettings> LastRoomSettings = Sessions ? Sessions->GetLastRoomSettings() : TOptional<FSavaRoomSettings>();
		LobbyGameState->SetRoomSettings(LastRoomSettings.IsSet() ? LobbySettings->Sanitize(LastRoomSettings.GetValue()) : LobbySettings->MakeDefaultRoomSettings());
	}
	else
	{
		UE_LOG(LogSavaLobby, Error, TEXT("GameState が ASavaLobbyGameState ではありません。GameMode の Game State Class を SavaLobbyGameState にしてください"));
	}
}

void ASavaLobbyGameMode::BeginPlay()
{
	Super::BeginPlay();

	//試合から戻ってきたら、また途中から入れるようにする(試合を始めるときに閉じている)
	if (USavaSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<USavaSessionSubsystem>())
	{
		Sessions->SetRoomJoinable(true);
	}
}

void ASavaLobbyGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (!ErrorMessage.IsEmpty())
	{
		return;
	}

	const ASavaLobbyGameState* LobbyGameState = GetLobbyGameState();
	if (LobbyGameState && LobbyGameState->IsMatchStarting())
	{
		ErrorMessage = TEXT("Match is starting.");
	}
}

void ASavaLobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	MarkRoomHost(NewPlayer);
}

void ASavaLobbyGameMode::InitSeamlessTravelPlayer(AController* NewController)
{
	//試合から戻ってきた人(準備 OK は押し直し)
	Super::InitSeamlessTravelPlayer(NewController);
	MarkRoomHost(NewController);
}

void ASavaLobbyGameMode::MarkRoomHost(AController* Controller)
{
	if (ASavaPlayerState* SavaPlayerState = Controller ? Controller->GetPlayerState<ASavaPlayerState>() : nullptr)
	{
		SavaPlayerState->SetRoomHost(ASavaPlayerController::IsHostController(Cast<APlayerController>(Controller)));
		SavaPlayerState->SetReady(false);
	}
}

bool ASavaLobbyGameMode::CheckHost(const ASavaPlayerController* Requester, FText& OutError) const
{
	if (!ASavaPlayerController::IsHostController(Requester))
	{
		OutError = FText::FromString(TEXT("ホストだけが操作できます"));
		return false;
	}
	return true;
}

bool ASavaLobbyGameMode::IsLobbyPlayer(const ASavaPlayerState* PlayerState) const
{
	return PlayerState && GameState && GameState->PlayerArray.Contains(PlayerState);
}

//--------------------------------チーム

bool ASavaLobbyGameMode::CanJoinTeam(const TArray<APlayerState*>& Players, const ASavaPlayerState* Player, uint8 TeamId, int32 InNumTeams, int32 InMaxPlayersPerTeam, FText& OutError)
{
	if (!Player)
	{
		return false;
	}
	if (TeamId >= InNumTeams)
	{
		OutError = FText::FromString(TEXT("そのチームはありません"));
		return false;
	}
	if (Player->GetTeamId() == TeamId)
	{
		OutError = FText::FromString(TEXT("すでにそのチームです"));
		return false;
	}
	if (ASavaLobbyGameState::CountTeamPlayers(Players, TeamId, Player) >= InMaxPlayersPerTeam)
	{
		OutError = FText::FromString(FString::Printf(TEXT("チーム %d は満員です"), TeamId + 1));
		return false;
	}
	return true;
}

bool ASavaLobbyGameMode::ChangeTeam(ASavaPlayerState* Player, uint8 TeamId, FText& OutError)
{
	const ASavaLobbyGameState* LobbyGameState = GetLobbyGameState();
	if (!LobbyGameState || LobbyGameState->IsMatchStarting())
	{
		OutError = FText::FromString(TEXT("今は変えられません"));
		return false;
	}
	if (!CanJoinTeam(LobbyGameState->PlayerArray, Player, TeamId, NumTeams, MaxPlayersPerTeam, OutError))
	{
		return false;
	}

	UE_LOG(LogSavaLobby, Log, TEXT("%s をチーム %d へ"), *Player->GetPlayerName(), TeamId);
	Player->SetTeamId(TeamId);
	return true;
}

bool ASavaLobbyGameMode::RequestJoinTeam(ASavaPlayerController* Requester, uint8 TeamId, FText& OutError)
{
	ASavaPlayerState* SavaPlayerState = Requester ? Requester->GetPlayerState<ASavaPlayerState>() : nullptr;
	return SavaPlayerState && ChangeTeam(SavaPlayerState, TeamId, OutError);
}

bool ASavaLobbyGameMode::RequestMovePlayerToTeam(ASavaPlayerController* Requester, ASavaPlayerState* Target, uint8 TeamId, FText& OutError)
{
	if (!CheckHost(Requester, OutError))
	{
		return false;
	}
	if (!IsLobbyPlayer(Target))
	{
		OutError = FText::FromString(TEXT("その人は部屋にいません"));
		return false;
	}
	return ChangeTeam(Target, TeamId, OutError);
}

//--------------------------------準備 OK

bool ASavaLobbyGameMode::RequestSetReady(ASavaPlayerController* Requester, bool bReady, FText& OutError)
{
	ASavaPlayerState* SavaPlayerState = Requester ? Requester->GetPlayerState<ASavaPlayerState>() : nullptr;
	const ASavaLobbyGameState* LobbyGameState = GetLobbyGameState();
	if (!SavaPlayerState || !LobbyGameState)
	{
		return false;
	}
	if (LobbyGameState->IsMatchStarting())
	{
		OutError = FText::FromString(TEXT("今は変えられません"));
		return false;
	}
	//ホストは開始ボタンを押す人なので、準備 OK は要らない
	if (SavaPlayerState->IsRoomHost())
	{
		OutError = FText::FromString(TEXT("ホストは準備 OK を押す必要はありません"));
		return false;
	}

	SavaPlayerState->SetReady(bReady);
	return true;
}

//--------------------------------ルーム設定・メンバー管理

bool ASavaLobbyGameMode::RequestUpdateRoomSettings(ASavaPlayerController* Requester, const FSavaRoomSettings& NewSettings, FText& OutError)
{
	ASavaLobbyGameState* LobbyGameState = GetLobbyGameState();
	if (!LobbyGameState || !CheckHost(Requester, OutError))
	{
		return false;
	}
	if (LobbyGameState->IsMatchStarting())
	{
		OutError = FText::FromString(TEXT("今は変えられません"));
		return false;
	}

	FSavaRoomSettings Sanitized = GetDefault<USavaLobbySettings>()->Sanitize(NewSettings);
	//モードを変えたら、前のモードのマップ番号は意味がないので最初のマップにする
	if (Sanitized.ModeIndex != LobbyGameState->GetRoomSettings().ModeIndex && NewSettings.MapIndex == LobbyGameState->GetRoomSettings().MapIndex)
	{
		Sanitized.MapIndex = 0;
	}

	LobbyGameState->SetRoomSettings(Sanitized);
	return true;
}

bool ASavaLobbyGameMode::RequestKickPlayer(ASavaPlayerController* Requester, ASavaPlayerState* Target, FText& OutError)
{
	if (!CheckHost(Requester, OutError))
	{
		return false;
	}
	if (!IsLobbyPlayer(Target))
	{
		OutError = FText::FromString(TEXT("その人は部屋にいません"));
		return false;
	}
	if (Target->IsRoomHost() || Target == Requester->GetPlayerState<ASavaPlayerState>())
	{
		OutError = FText::FromString(TEXT("ホストは追い出せません"));
		return false;
	}

	APlayerController* TargetController = Cast<APlayerController>(Target->GetOwner());
	UE_LOG(LogSavaLobby, Log, TEXT("%s を追い出します"), *Target->GetPlayerName());
	if (!TargetController || !GameSession || !GameSession->KickPlayer(TargetController, FText::FromString(TEXT("ホストに部屋から追い出されました"))))
	{
		OutError = FText::FromString(TEXT("追い出せませんでした"));
		return false;
	}
	return true;
}

//--------------------------------ゲーム開始

FString ASavaLobbyGameMode::BuildMatchTravelURL(const FSavaRoomSettings& RoomSettings, int32 NumPlayers)
{
	const FSavaMatchModeOption* Mode = GetDefault<USavaLobbySettings>()->FindMode(RoomSettings.ModeIndex);
	if (!Mode || Mode->GameModeClass.IsNull() || !Mode->Maps.IsValidIndex(RoomSettings.MapIndex) || Mode->Maps[RoomSettings.MapIndex].Map.IsNull())
	{
		return FString();
	}

	//例: /Game/Map/FPSMap?listen?game=/Script/sava.SavaCTFGameMode?Rounds=2?TimeLimit=180?Players=6
	FString URL = Mode->Maps[RoomSettings.MapIndex].Map.GetLongPackageName();
	URL += TEXT("?listen");
	URL += FString::Printf(TEXT("?game=%s"), *Mode->GameModeClass.ToString());
	if (Mode->bUsesRounds)
	{
		URL += FString::Printf(TEXT("?%s=%d"), SavaMatchOptions::RoundsToWin, RoomSettings.RoundsToWin);
	}
	if (Mode->bUsesKillLimit)
	{
		URL += FString::Printf(TEXT("?%s=%d"), SavaMatchOptions::KillLimit, RoomSettings.KillLimit);
	}
	if (Mode->bCanChooseTeamMatch)
	{
		URL += FString::Printf(TEXT("?%s=%d"), SavaMatchOptions::TeamMatch, RoomSettings.bTeamMatch ? 1 : 0);
	}
	URL += FString::Printf(TEXT("?%s=%d"), SavaMatchOptions::TimeLimit, RoomSettings.RoundTimeLimit);
	URL += FString::Printf(TEXT("?%s=%d"), SavaMatchOptions::ExpectedPlayers, NumPlayers);
	return URL;
}

bool ASavaLobbyGameMode::RequestStartMatch(ASavaPlayerController* Requester, FText& OutError)
{
	ASavaLobbyGameState* LobbyGameState = GetLobbyGameState();
	if (!LobbyGameState || !CheckHost(Requester, OutError))
	{
		return false;
	}
	if (!LobbyGameState->CanStartMatch(OutError))
	{
		return false;
	}

	UE_LOG(LogSavaLobby, Log, TEXT("試合を始めます"));
	LobbyGameState->SetMatchStarting(true);

	//試合中は部屋に入れないようにする(検索にも出さない)。ルーム設定は戻ってきたときのために覚えておく
	if (USavaSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<USavaSessionSubsystem>())
	{
		Sessions->SetRoomJoinable(false);
		Sessions->SetLastRoomSettings(LobbyGameState->GetRoomSettings());
	}

	GetWorldTimerManager().SetTimer(StartMatchTimer, this, &ThisClass::TravelToMatch, FMath::Max(StartMatchDelay, 0.01f), false);
	return true;
}

void ASavaLobbyGameMode::TravelToMatch()
{
	ASavaLobbyGameState* LobbyGameState = GetLobbyGameState();
	const FString URL = LobbyGameState ? BuildMatchTravelURL(LobbyGameState->GetRoomSettings(), GetNumPlayers()) : FString();
	if (URL.IsEmpty())
	{
		//待っている間に設定が壊れた(ありえないが念のため)。部屋を開き直す
		UE_LOG(LogSavaLobby, Error, TEXT("試合マップへ移れません(ルーム設定のモード・マップを確認してください)"));
		if (LobbyGameState)
		{
			LobbyGameState->SetMatchStarting(false);
		}
		if (USavaSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<USavaSessionSubsystem>())
		{
			Sessions->SetRoomJoinable(true);
		}
		return;
	}

	UE_LOG(LogSavaLobby, Log, TEXT("試合マップへ移ります: %s"), *URL);
	GetWorld()->ServerTravel(URL);
}

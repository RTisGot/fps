#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Lobby/SavaInteractButton.h"
#include "Lobby/SavaLobbyGameMode.h"
#include "Lobby/SavaLobbyGameState.h"
#include "Lobby/SavaLobbySettings.h"
#include "Player/SavaPlayerController.h"
#include "Player/SavaPlayerState.h"

namespace
{
	//テスト用の一時的なワールド(スコープを抜けると破棄する)
	struct FSavaLobbyTestWorld
	{
		UWorld* World = nullptr;

		FSavaLobbyTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
		}

		~FSavaLobbyTestWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		ASavaPlayerState* SpawnPlayer(uint8 TeamId, bool bHost = false, bool bReady = false) const
		{
			ASavaPlayerState* PlayerState = World->SpawnActor<ASavaPlayerState>();
			PlayerState->SetTeamId(TeamId);
			PlayerState->SetRoomHost(bHost);
			PlayerState->SetReady(bReady);
			return PlayerState;
		}
	};

	FSavaMatchModeOption MakeMode(int32 MinPlayers, bool bPlayable, int32 NumMaps = 1)
	{
		FSavaMatchModeOption Mode;
		Mode.MinPlayers = MinPlayers;
		Mode.bUsesRounds = true;
		if (bPlayable)
		{
			Mode.GameModeClass = AGameModeBase::StaticClass();
		}
		for (int32 Index = 0; Index < NumMaps; ++Index)
		{
			FSavaMatchMapOption& Map = Mode.Maps.AddDefaulted_GetRef();
			Map.Map = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Map/FPSMap.FPSMap")));
		}
		return Mode;
	}

	//設定はテストの中だけで差し替え、終わったら戻す
	struct FScopedLobbyModes
	{
		TArray<FSavaMatchModeOption> SavedModes;

		explicit FScopedLobbyModes(const TArray<FSavaMatchModeOption>& Modes)
		{
			USavaLobbySettings* Settings = GetMutableDefault<USavaLobbySettings>();
			SavedModes = Settings->Modes;
			Settings->Modes = Modes;
		}

		~FScopedLobbyModes()
		{
			GetMutableDefault<USavaLobbySettings>()->Modes = SavedModes;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaLobbySanitizeRoomSettingsTest, "Sava.Lobby.SanitizeRoomSettings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSavaLobbySanitizeRoomSettingsTest::RunTest(const FString&)
{
	USavaLobbySettings* Settings = NewObject<USavaLobbySettings>();
	Settings->Modes = { MakeMode(4, true, 2), MakeMode(2, false, 1) };
	Settings->MinRoundsToWin = 1;
	Settings->MaxRoundsToWin = 5;
	Settings->MinRoundTimeLimit = 60;
	Settings->MaxRoundTimeLimit = 600;
	Settings->MinKillLimit = 5;
	Settings->MaxKillLimit = 100;

	FSavaRoomSettings Valid;
	Valid.ModeIndex = 0;
	Valid.MapIndex = 1;
	Valid.RoundsToWin = 3;
	Valid.RoundTimeLimit = 120;
	Valid.KillLimit = 20;
	Valid.bTeamMatch = true;
	TestTrue(TEXT("Valid settings are kept"), Settings->Sanitize(Valid) == Valid);

	FSavaRoomSettings Broken;
	Broken.ModeIndex = 7;
	Broken.MapIndex = 9;
	Broken.RoundsToWin = 99;
	Broken.RoundTimeLimit = 5;
	Broken.KillLimit = 999;
	const FSavaRoomSettings Fixed = Settings->Sanitize(Broken);
	TestEqual(TEXT("Kill limit is clamped"), Fixed.KillLimit, 100);
	TestEqual(TEXT("Unknown mode falls back to the first"), Fixed.ModeIndex, 0);
	TestEqual(TEXT("Unknown map falls back to the first"), Fixed.MapIndex, 0);
	TestEqual(TEXT("Rounds are clamped"), Fixed.RoundsToWin, 5);
	TestEqual(TEXT("Time limit is clamped"), Fixed.RoundTimeLimit, 60);

	FSavaRoomSettings SecondMode;
	SecondMode.ModeIndex = 1;
	SecondMode.MapIndex = 1; //2 番目のモードにはマップが 1 つしかない
	TestEqual(TEXT("Map index is checked against the chosen mode"), Settings->Sanitize(SecondMode).MapIndex, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaLobbyCanStartMatchTest, "Sava.Lobby.CanStartMatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSavaLobbyCanStartMatchTest::RunTest(const FString&)
{
	FSavaLobbyTestWorld TestWorld;
	USavaLobbySettings* Settings = NewObject<USavaLobbySettings>();
	Settings->Modes = { MakeMode(4, true), MakeMode(2, false) };

	FSavaRoomSettings Ctf;
	Ctf.ModeIndex = 0;
	FText Reason;

	//ホスト(準備 OK なし) + 2 人(準備 OK)= 3 人。CTF は 4 人から
	TArray<APlayerState*> Players = {
		TestWorld.SpawnPlayer(0, true, false),
		TestWorld.SpawnPlayer(1, false, true),
		TestWorld.SpawnPlayer(0, false, true),
	};
	TestFalse(TEXT("Too few players"), Settings->CanStartMatch(Ctf, Players, Reason));

	ASavaPlayerState* Late = TestWorld.SpawnPlayer(1, false, false);
	Players.Add(Late);
	TestFalse(TEXT("A non-host player is not ready"), Settings->CanStartMatch(Ctf, Players, Reason));

	Late->SetReady(true);
	TestTrue(TEXT("Host does not need to be ready"), Settings->CanStartMatch(Ctf, Players, Reason));

	FSavaRoomSettings Deathmatch;
	Deathmatch.ModeIndex = 1;
	TestFalse(TEXT("A mode without a GameMode cannot start"), Settings->CanStartMatch(Deathmatch, Players, Reason));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaLobbyCanJoinTeamTest, "Sava.Lobby.CanJoinTeam",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSavaLobbyCanJoinTeamTest::RunTest(const FString&)
{
	FSavaLobbyTestWorld TestWorld;
	ASavaPlayerState* Mover = TestWorld.SpawnPlayer(0);
	TArray<APlayerState*> Players = { Mover, TestWorld.SpawnPlayer(1), TestWorld.SpawnPlayer(1) };
	FText Error;

	TestTrue(TEXT("Can join a team with room"), ASavaLobbyGameMode::CanJoinTeam(Players, Mover, 1, 2, 3, Error));
	TestFalse(TEXT("Cannot join a full team"), ASavaLobbyGameMode::CanJoinTeam(Players, Mover, 1, 2, 2, Error));
	TestFalse(TEXT("Cannot join the same team"), ASavaLobbyGameMode::CanJoinTeam(Players, Mover, 0, 2, 3, Error));
	TestFalse(TEXT("Cannot join a team that does not exist"), ASavaLobbyGameMode::CanJoinTeam(Players, Mover, 2, 2, 3, Error));
	TestEqual(TEXT("Team count ignores the excluded player"), ASavaLobbyGameState::CountTeamPlayers(Players, 0, Mover), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaLobbyTravelURLTest, "Sava.Lobby.MatchTravelURL",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSavaLobbyTravelURLTest::RunTest(const FString&)
{
	FScopedLobbyModes Modes({ MakeMode(4, true), MakeMode(2, false) });

	FSavaRoomSettings RoomSettings;
	RoomSettings.ModeIndex = 0;
	RoomSettings.RoundsToWin = 3;
	RoomSettings.RoundTimeLimit = 120;
	const FString URL = ASavaLobbyGameMode::BuildMatchTravelURL(RoomSettings, 6);
	TestTrue(TEXT("Starts with the map"), URL.StartsWith(TEXT("/Game/Map/FPSMap?")));
	TestTrue(TEXT("Keeps listening"), URL.Contains(TEXT("?listen")));
	TestTrue(TEXT("Has the GameMode"), URL.Contains(TEXT("?game=/Script/Engine.GameModeBase")));
	TestTrue(TEXT("Has the rounds"), URL.Contains(TEXT("?Rounds=3")));
	TestTrue(TEXT("Has the time limit"), URL.Contains(TEXT("?TimeLimit=120")));
	TestTrue(TEXT("Has the player count"), URL.Contains(TEXT("?Players=6")));
	TestFalse(TEXT("No kill limit for a mode that does not use it"), URL.Contains(TEXT("?Kills=")));
	TestFalse(TEXT("No team option for a mode that cannot choose it"), URL.Contains(TEXT("?Teams=")));

	RoomSettings.ModeIndex = 1;
	TestTrue(TEXT("No URL for a mode without a GameMode"), ASavaLobbyGameMode::BuildMatchTravelURL(RoomSettings, 6).IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaLobbyUsesTeamsTest, "Sava.Lobby.UsesTeams",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSavaLobbyUsesTeamsTest::RunTest(const FString&)
{
	FSavaMatchModeOption Deathmatch = MakeMode(2, true);
	Deathmatch.bCanChooseTeamMatch = true;
	FScopedLobbyModes Modes({ MakeMode(4, true), Deathmatch });

	FSavaRoomSettings RoomSettings;
	RoomSettings.ModeIndex = 0;
	RoomSettings.bTeamMatch = false;
	TestTrue(TEXT("A mode that cannot choose is always a team match"), USavaLobbySettings::UsesTeams(RoomSettings));

	RoomSettings.ModeIndex = 1;
	TestFalse(TEXT("Free-for-all has no teams"), USavaLobbySettings::UsesTeams(RoomSettings));
	RoomSettings.bTeamMatch = true;
	TestTrue(TEXT("Team match has teams"), USavaLobbySettings::UsesTeams(RoomSettings));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaLobbyKillLimitURLTest, "Sava.Lobby.MatchTravelURLKillsAndTeams",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSavaLobbyKillLimitURLTest::RunTest(const FString&)
{
	FSavaMatchModeOption Deathmatch = MakeMode(2, true);
	Deathmatch.bUsesRounds = false;
	Deathmatch.bUsesKillLimit = true;
	Deathmatch.bCanChooseTeamMatch = true;
	FScopedLobbyModes Modes({ Deathmatch });

	FSavaRoomSettings RoomSettings;
	RoomSettings.KillLimit = 15;
	RoomSettings.bTeamMatch = true;
	FString URL = ASavaLobbyGameMode::BuildMatchTravelURL(RoomSettings, 4);
	TestFalse(TEXT("No rounds for a mode without rounds"), URL.Contains(TEXT("?Rounds=")));
	TestTrue(TEXT("Has the kill limit"), URL.Contains(TEXT("?Kills=15")));
	TestTrue(TEXT("Has team match"), URL.Contains(TEXT("?Teams=1")));

	RoomSettings.bTeamMatch = false;
	URL = ASavaLobbyGameMode::BuildMatchTravelURL(RoomSettings, 4);
	TestTrue(TEXT("Has free-for-all"), URL.Contains(TEXT("?Teams=0")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaLobbyReadyButtonHostTest, "Sava.Lobby.ReadyButtonNotForHost",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSavaLobbyReadyButtonHostTest::RunTest(const FString&)
{
	FSavaLobbyTestWorld TestWorld;
	//1 人で動かしているワールドでは、自分の PlayerController がホスト
	APlayerController* HostController = TestWorld.World->SpawnActor<APlayerController>();
	ASavaInteractButton* Button = TestWorld.World->SpawnActor<ASavaInteractButton>();
	TestTrue(TEXT("The local controller is the host"), ASavaPlayerController::IsHostController(HostController));

	//Action は配置したものごとに Details で選ぶ値なので、テストではリフレクションで入れる
	const FEnumProperty* ActionProperty = FindFProperty<FEnumProperty>(ASavaInteractButton::StaticClass(), TEXT("Action"));
	if (!TestNotNull(TEXT("Action property"), ActionProperty))
	{
		return false;
	}
	ESavaInteractButtonAction* Action = ActionProperty->ContainerPtrToValuePtr<ESavaInteractButtonAction>(Button);

	*Action = ESavaInteractButtonAction::ToggleReady;
	TestFalse(TEXT("Host cannot use the ready button"), Button->CanInteract(HostController));

	*Action = ESavaInteractButtonAction::JoinTeam;
	TestTrue(TEXT("Host can use the team button"), Button->CanInteract(HostController));

	*Action = ESavaInteractButtonAction::StartMatch;
	TestTrue(TEXT("Start match is host only"), Button->IsHostOnly());
	TestTrue(TEXT("Host can use the start button"), Button->CanInteract(HostController));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSavaLobbyCopyPropertiesTest, "Sava.Lobby.CopyPropertiesKeepsTeam",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSavaLobbyCopyPropertiesTest::RunTest(const FString&)
{
	FSavaLobbyTestWorld TestWorld;
	ASavaPlayerState* LobbyState = TestWorld.SpawnPlayer(1, true, true);
	ASavaPlayerState* MatchState = TestWorld.SpawnPlayer(ASavaPlayerState::NoTeam);

	LobbyState->DispatchCopyProperties(MatchState);
	TestEqual(TEXT("Team is carried into the match"), MatchState->GetTeamId(), static_cast<uint8>(1));
	TestTrue(TEXT("Host mark is carried into the match"), MatchState->IsRoomHost());
	TestFalse(TEXT("Ready is not carried"), MatchState->IsReady());

	return true;
}

#endif

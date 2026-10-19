// Fill out your copyright notice in the Description page of Project Settings.

//ロビーのテスト用コンソールコマンド(UI ができるまでの確認用。` キーでコンソールを開いて打つ)
//どれも自分の PlayerController からサーバーへ頼む(ボタン・端末を押したのと同じ)

#include "Lobby/SavaLobbyGameState.h"
#include "Lobby/SavaLobbySettings.h"
#include "Player/SavaPlayerController.h"
#include "Player/SavaPlayerState.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogSavaLobbyCommand, Log, All);

namespace SavaLobbyCommand
{
	static ASavaPlayerController* GetLocalController(UWorld* World)
	{
		return World ? Cast<ASavaPlayerController>(World->GetFirstPlayerController()) : nullptr;
	}

	static ASavaPlayerState* FindPlayerByName(UWorld* World, const FString& Name)
	{
		const ASavaLobbyGameState* LobbyGameState = World ? World->GetGameState<ASavaLobbyGameState>() : nullptr;
		if (!LobbyGameState)
		{
			return nullptr;
		}
		for (ASavaPlayerState* PlayerState : LobbyGameState->GetLobbyPlayers())
		{
			if (PlayerState->GetPlayerName().Equals(Name, ESearchCase::IgnoreCase))
			{
				return PlayerState;
			}
		}
		return nullptr;
	}

	static FAutoConsoleCommandWithWorldAndArgs JoinTeamCommand(
		TEXT("Sava.JoinTeam"), TEXT("チームに入る(0 から)。例: Sava.JoinTeam 1"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ASavaPlayerController* Controller = GetLocalController(World))
			{
				Controller->ServerJoinTeam(static_cast<uint8>(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0));
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs ReadyCommand(
		TEXT("Sava.Ready"), TEXT("準備 OK にする(0 で解除)。例: Sava.Ready / Sava.Ready 0"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ASavaPlayerController* Controller = GetLocalController(World))
			{
				Controller->ServerSetReady(Args.Num() == 0 || FCString::Atoi(*Args[0]) != 0);
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs RoomSettingsCommand(
		TEXT("Sava.RoomSettings"), TEXT("ホストだけ: ルーム設定を変える。例: Sava.RoomSettings <モード番号> <マップ番号> <勝利ラウンド数> <制限時間(秒)> <勝利キル数> <チーム戦なら 1>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ASavaPlayerController* Controller = GetLocalController(World);
			const ASavaLobbyGameState* LobbyGameState = World ? World->GetGameState<ASavaLobbyGameState>() : nullptr;
			if (!Controller || !LobbyGameState)
			{
				return;
			}

			//書かなかった項目は今の値のまま
			FSavaRoomSettings Settings = LobbyGameState->GetRoomSettings();
			if (Args.Num() > 0) Settings.ModeIndex = FCString::Atoi(*Args[0]);
			if (Args.Num() > 1) Settings.MapIndex = FCString::Atoi(*Args[1]);
			if (Args.Num() > 2) Settings.RoundsToWin = FCString::Atoi(*Args[2]);
			if (Args.Num() > 3) Settings.RoundTimeLimit = FCString::Atoi(*Args[3]);
			if (Args.Num() > 4) Settings.KillLimit = FCString::Atoi(*Args[4]);
			if (Args.Num() > 5) Settings.bTeamMatch = FCString::Atoi(*Args[5]) != 0;
			Controller->ServerUpdateRoomSettings(Settings);
		}));

	static FAutoConsoleCommandWithWorldAndArgs MovePlayerCommand(
		TEXT("Sava.MovePlayer"), TEXT("ホストだけ: 他の人のチームを変える。例: Sava.MovePlayer Taro 1"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ASavaPlayerController* Controller = GetLocalController(World);
			ASavaPlayerState* Target = Args.Num() > 0 ? FindPlayerByName(World, Args[0]) : nullptr;
			if (Controller && Target)
			{
				Controller->ServerMovePlayerToTeam(Target, static_cast<uint8>(Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 0));
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs KickCommand(
		TEXT("Sava.Kick"), TEXT("ホストだけ: 他の人を部屋から追い出す。例: Sava.Kick Taro"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ASavaPlayerController* Controller = GetLocalController(World);
			ASavaPlayerState* Target = Args.Num() > 0 ? FindPlayerByName(World, Args[0]) : nullptr;
			if (Controller && Target)
			{
				Controller->ServerKickPlayer(Target);
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs StartMatchCommand(
		TEXT("Sava.StartMatch"), TEXT("ホストだけ: 試合を始める"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ASavaPlayerController* Controller = GetLocalController(World))
			{
				Controller->ServerStartMatch();
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs LobbyStatusCommand(
		TEXT("Sava.LobbyStatus"), TEXT("ロビーの状態(ルーム設定・全員のチームと準備 OK・開始できるか)を Output Log に出す"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const ASavaLobbyGameState* LobbyGameState = World ? World->GetGameState<ASavaLobbyGameState>() : nullptr;
			if (!LobbyGameState)
			{
				UE_LOG(LogSavaLobbyCommand, Log, TEXT("ロビーではありません(%s)"), World ? *World->GetMapName() : TEXT("-"));
				return;
			}

			const FSavaRoomSettings& Settings = LobbyGameState->GetRoomSettings();
			FText ModeName, MapName;
			USavaLobbySettings::GetRoomSettingsDisplayNames(Settings, ModeName, MapName);
			UE_LOG(LogSavaLobbyCommand, Log, TEXT("ルーム設定: %s / %s / %d ラウンド先取 / %d 秒 / %d キル / %s"),
				*ModeName.ToString(), *MapName.ToString(), Settings.RoundsToWin, Settings.RoundTimeLimit,
				Settings.KillLimit, Settings.bTeamMatch ? TEXT("チーム戦") : TEXT("個人戦"));

			for (const ASavaPlayerState* PlayerState : LobbyGameState->GetLobbyPlayers())
			{
				UE_LOG(LogSavaLobbyCommand, Log, TEXT("  %s: チーム %d%s%s"), *PlayerState->GetPlayerName(), PlayerState->GetTeamId(),
					PlayerState->IsRoomHost() ? TEXT(" [ホスト]") : TEXT(""), PlayerState->IsReady() ? TEXT(" [準備 OK]") : TEXT(""));
			}

			FText Reason;
			const bool bCanStart = LobbyGameState->CanStartMatch(Reason);
			UE_LOG(LogSavaLobbyCommand, Log, TEXT("開始: %s"), bCanStart ? TEXT("できる") : *Reason.ToString());
		}));

	static FAutoConsoleCommandWithWorldAndArgs LocalStatusCommand(
		TEXT("Sava.LocalStatus"), TEXT("自分の PlayerController・体・表示中の画面を Output Log に出す(マップ移動の後に HUD や操作が来ているかの確認用)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
			const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
			UE_LOG(LogSavaLobbyCommand, Log, TEXT("マップ %s / PlayerController %s(LocalPlayer %s)/ 体 %s"),
				World ? *World->GetMapName() : TEXT("-"), *GetNameSafe(Controller),
				Controller && Controller->GetLocalPlayer() ? TEXT("あり") : TEXT("なし"), *GetNameSafe(Pawn));

			TArray<UUserWidget*> Widgets;
			UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Widgets, UUserWidget::StaticClass(), true);
			for (const UUserWidget* Widget : Widgets)
			{
				if (Widget->IsInViewport())
				{
					UE_LOG(LogSavaLobbyCommand, Log, TEXT("  表示中: %s"), *Widget->GetClass()->GetName());
				}
			}
		}));

	//指定した秒数の後にコマンドを打つ(起動時の -ExecCmds で、部屋を作る → 開始 などを順に試すため)
	//マップが変わっても続くように、ワールドではなくエンジン全体のタイマーを使う
	static FAutoConsoleCommandWithWorldAndArgs AfterCommand(
		TEXT("Sava.After"), TEXT("秒数の後にコマンドを打つ。例: Sava.After 5 Sava.StartMatch"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() < 2)
			{
				return;
			}

			const float Delay = FCString::Atof(*Args[0]);
			const FString Command = FString::Join(TArrayView<const FString>(Args).RightChop(1), TEXT(" "));
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Command](float)
			{
				//その時点のゲームのワールドで打つ
				for (const FWorldContext& Context : GEngine->GetWorldContexts())
				{
					UWorld* GameWorld = Context.World();
					if (GameWorld && (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE))
					{
						UE_LOG(LogSavaLobbyCommand, Log, TEXT("Sava.After: %s"), *Command);
						GEngine->Exec(GameWorld, *Command);
						break;
					}
				}
				return false; //1 回だけ
			}), Delay);
		}));
}

// Fill out your copyright notice in the Description page of Project Settings.

#include "Online/SavaSessionSubsystem.h"
#include "Online/SavaOnlineSettings.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSubsystemUtils.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "GameFramework/PlayerController.h"
#include "GameMapsSettings.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogSavaSession, Log, All);

namespace SavaSession
{
	//部屋名を張り紙(セッション設定)に載せるときのキー
	const FName RoomNameKey(TEXT("SAVA_ROOMNAME"));
}

void USavaSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	//切断・接続失敗はエンジン全体のイベントで届く(PIE では他の PIE インスタンスの分も来るので、受け取り側で自分の分か確かめる)
	NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &ThisClass::HandleNetworkFailure);
	TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &ThisClass::HandleTravelFailure);
}

void USavaSessionSubsystem::Deinitialize()
{
	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	}

	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionHandle);
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsHandle);
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionHandle);
	}

	Super::Deinitialize();
}

IOnlineSessionPtr USavaSessionSubsystem::GetSessionInterface() const
{
	//World を渡すと、PIE ではそのインスタンス専用の Online Subsystem が返る
	IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(GetWorld());
	return OnlineSubsystem ? OnlineSubsystem->GetSessionInterface() : nullptr;
}

bool USavaSessionSubsystem::IsLANSubsystem() const
{
	const IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(GetWorld());
	return OnlineSubsystem && OnlineSubsystem->GetSubsystemName() == NULL_SUBSYSTEM;
}

bool USavaSessionSubsystem::IsInRoom() const
{
	const IOnlineSessionPtr Sessions = GetSessionInterface();
	return Sessions.IsValid() && Sessions->GetNamedSession(NAME_GameSession) != nullptr;
}

void USavaSessionSubsystem::DestroySessionThen(TFunction<void()> Next)
{
	const IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid() || Sessions->GetNamedSession(NAME_GameSession) == nullptr)
	{
		Next();
		return;
	}

	UE_LOG(LogSavaSession, Log, TEXT("前の部屋を消してから続けます"));
	const bool bStarted = Sessions->DestroySession(NAME_GameSession,
		FOnDestroySessionCompleteDelegate::CreateWeakLambda(this, [Next](FName, bool)
		{
			Next();
		}));

	//消す処理を始められなかったとき(完了の通知は来ない)
	if (!bStarted)
	{
		Next();
	}
}

// ---------------------------------------------------------------------------
// 部屋を作る
// ---------------------------------------------------------------------------

void USavaSessionSubsystem::CreateRoom(const FString& RoomName)
{
	if (bBusy)
	{
		return;
	}

	if (!GetSessionInterface().IsValid())
	{
		UE_LOG(LogSavaSession, Error, TEXT("Online Subsystem が使えません"));
		OnCreateRoomComplete.Broadcast(false);
		return;
	}

	bBusy = true;
	DestroySessionThen([this, RoomName]()
	{
		StartCreateSession(RoomName);
	});
}

void USavaSessionSubsystem::StartCreateSession(FString RoomName)
{
	const IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		bBusy = false;
		OnCreateRoomComplete.Broadcast(false);
		return;
	}

	if (RoomName.IsEmpty())
	{
		RoomName = TEXT("Room");
	}

	const USavaOnlineSettings* Settings = GetDefault<USavaOnlineSettings>();

	FOnlineSessionSettings SessionSettings;
	SessionSettings.NumPublicConnections = Settings->MaxPlayers;
	SessionSettings.bShouldAdvertise = true;		//検索に出す
	SessionSettings.bAllowJoinInProgress = true;	//ロビー中は途中から入れる(試合が始まったら止める)
	SessionSettings.bIsLANMatch = IsLANSubsystem();
	SessionSettings.bIsDedicated = false;			//ホストも遊ぶ listen サーバー
	SessionSettings.bUsesPresence = true;			//EOS ではロビー(Lobbies)として作るのに必要
	SessionSettings.bUseLobbiesIfAvailable = true;
	SessionSettings.bAllowJoinViaPresence = true;
	SessionSettings.Set(SavaSession::RoomNameKey, RoomName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	CreateSessionHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleCreateSessionComplete));

	UE_LOG(LogSavaSession, Log, TEXT("部屋を作ります: %s"), *RoomName);
	if (!Sessions->CreateSession(0, NAME_GameSession, SessionSettings))
	{
		//始められなかったとき(完了の通知が来ないことがあるので、ここで終わらせる)
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionHandle);
		bBusy = false;
		OnCreateRoomComplete.Broadcast(false);
	}
}

void USavaSessionSubsystem::HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (const IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionHandle);
	}
	bBusy = false;

	UE_LOG(LogSavaSession, Log, TEXT("部屋の作成: %s"), bWasSuccessful ? TEXT("成功") : TEXT("失敗"));
	OnCreateRoomComplete.Broadcast(bWasSuccessful);

	if (!bWasSuccessful)
	{
		return;
	}

	const TSoftObjectPtr<UWorld>& LobbyMap = GetDefault<USavaOnlineSettings>()->LobbyMap;
	if (LobbyMap.IsNull())
	{
		UE_LOG(LogSavaSession, Error, TEXT("Lobby Map が未設定です(Project Settings → Game → Sava Online)"));
		return;
	}

	//listen を付けて開くと、このマップのサーバーになって他の人が接続できるようになる
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, LobbyMap, true, TEXT("listen"));
}

// ---------------------------------------------------------------------------
// 部屋を探す
// ---------------------------------------------------------------------------

void USavaSessionSubsystem::FindRooms()
{
	const IOnlineSessionPtr Sessions = GetSessionInterface();
	if (bBusy || !Sessions.IsValid())
	{
		if (!Sessions.IsValid())
		{
			OnFindRoomsComplete.Broadcast(false, {});
		}
		return;
	}

	bBusy = true;

	LastSearch = MakeShared<FOnlineSessionSearch>();
	LastSearch->MaxSearchResults = GetDefault<USavaOnlineSettings>()->MaxSearchResults;
	LastSearch->bIsLanQuery = IsLANSubsystem();
	//EOS ではロビー(Lobbies)を探すという指定。Null では無視される
	LastSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);

	FindSessionsHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::HandleFindSessionsComplete));

	UE_LOG(LogSavaSession, Log, TEXT("部屋を探します"));
	if (!Sessions->FindSessions(0, LastSearch.ToSharedRef()))
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsHandle);
		bBusy = false;
		OnFindRoomsComplete.Broadcast(false, {});
	}
}

void USavaSessionSubsystem::HandleFindSessionsComplete(bool bWasSuccessful)
{
	if (const IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsHandle);
	}
	bBusy = false;

	TArray<FSavaRoomInfo> Rooms;
	if (bWasSuccessful && LastSearch.IsValid())
	{
		//エンジンの検索結果は BP で扱えないので、UI に要る分だけ FSavaRoomInfo に詰め直す
		for (int32 Index = 0; Index < LastSearch->SearchResults.Num(); ++Index)
		{
			const FOnlineSessionSearchResult& Result = LastSearch->SearchResults[Index];
			const FOnlineSession& Session = Result.Session;

			FSavaRoomInfo& Room = Rooms.AddDefaulted_GetRef();
			Room.SearchIndex = Index;
			Session.SessionSettings.Get(SavaSession::RoomNameKey, Room.RoomName);
			Room.HostName = Session.OwningUserName;
			Room.MaxPlayers = Session.SessionSettings.NumPublicConnections;
			Room.CurrentPlayers = Room.MaxPlayers - Session.NumOpenPublicConnections;
			Room.PingMs = Result.PingInMs;
		}
	}

	UE_LOG(LogSavaSession, Log, TEXT("部屋の検索: %s, %d 件"), bWasSuccessful ? TEXT("成功") : TEXT("失敗"), Rooms.Num());
	for (const FSavaRoomInfo& Room : Rooms)
	{
		UE_LOG(LogSavaSession, Log, TEXT("  [%d] %s (ホスト %s, %d/%d 人, %d ms)"),
			Room.SearchIndex, *Room.RoomName, *Room.HostName, Room.CurrentPlayers, Room.MaxPlayers, Room.PingMs);
	}

	OnFindRoomsComplete.Broadcast(bWasSuccessful, Rooms);
}

// ---------------------------------------------------------------------------
// 部屋に入る
// ---------------------------------------------------------------------------

void USavaSessionSubsystem::JoinRoom(int32 SearchIndex)
{
	if (bBusy)
	{
		return;
	}

	if (!LastSearch.IsValid() || !LastSearch->SearchResults.IsValidIndex(SearchIndex))
	{
		UE_LOG(LogSavaSession, Warning, TEXT("部屋 %d は直前の検索結果にありません"), SearchIndex);
		OnJoinRoomComplete.Broadcast(false);
		return;
	}

	bBusy = true;
	//検索結果は次の FindRooms で消えるのでコピーしておく
	const FOnlineSessionSearchResult SearchResult = LastSearch->SearchResults[SearchIndex];
	DestroySessionThen([this, SearchResult]()
	{
		StartJoinSession(SearchResult);
	});
}

void USavaSessionSubsystem::StartJoinSession(FOnlineSessionSearchResult SearchResult)
{
	const IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		bBusy = false;
		OnJoinRoomComplete.Broadcast(false);
		return;
	}

	JoinSessionHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleJoinSessionComplete));

	UE_LOG(LogSavaSession, Log, TEXT("部屋に入ります(ホスト %s)"), *SearchResult.Session.OwningUserName);
	if (!Sessions->JoinSession(0, NAME_GameSession, SearchResult))
	{
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionHandle);
		bBusy = false;
		OnJoinRoomComplete.Broadcast(false);
	}
}

void USavaSessionSubsystem::HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	const IOnlineSessionPtr Sessions = GetSessionInterface();
	if (Sessions.IsValid())
	{
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionHandle);
	}
	bBusy = false;

	//部屋に入れたら、張り紙に書かれたホストの連絡先(アドレス)を受け取って、そこへ接続しに行く
	FString ConnectString;
	APlayerController* PlayerController = GetGameInstance()->GetFirstLocalPlayerController();
	const bool bSuccess = Result == EOnJoinSessionCompleteResult::Success
		&& Sessions.IsValid()
		&& Sessions->GetResolvedConnectString(SessionName, ConnectString)
		&& PlayerController != nullptr;

	UE_LOG(LogSavaSession, Log, TEXT("部屋への参加: %s (結果 %s, 接続先 %s)"),
		bSuccess ? TEXT("成功") : TEXT("失敗"), LexToString(Result), *ConnectString);

	if (!bSuccess)
	{
		//入れなかったのに部屋の情報だけ残らないように消しておく
		DestroySessionThen([]() {});
		OnJoinRoomComplete.Broadcast(false);
		return;
	}

	OnJoinRoomComplete.Broadcast(true);
	PlayerController->ClientTravel(ConnectString, TRAVEL_Absolute);
}

// ---------------------------------------------------------------------------
// 部屋を抜ける
// ---------------------------------------------------------------------------

void USavaSessionSubsystem::LeaveRoom()
{
	UE_LOG(LogSavaSession, Log, TEXT("部屋を抜けます"));
	DestroySessionThen([this]()
	{
		//タイトルを開き直す。ホストならここで listen サーバーが閉じ、参加者は切断されてタイトルへ戻される
		const FString TitleMap = UGameMapsSettings::GetGameDefaultMap();
		UGameplayStatics::OpenLevel(this, FName(*TitleMap), true);
	});
}

// ---------------------------------------------------------------------------
// 切断
// ---------------------------------------------------------------------------

void USavaSessionSubsystem::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	//自分のゲーム(PIE では自分のインスタンス)の、参加者側の接続が切れたときだけ扱う
	//ホスト側でも「ある参加者との接続が切れた」で届くが、それは部屋が無くなったわけではない
	if (World == nullptr || World->GetGameInstance() != GetGameInstance())
	{
		return;
	}
	if (NetDriver == nullptr || NetDriver->GetNetMode() != NM_Client)
	{
		return;
	}

	HandleConnectionLost(FString::Printf(TEXT("%s: %s"), ENetworkFailure::ToString(FailureType), *ErrorString));
}

void USavaSessionSubsystem::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	if (World == nullptr || World->GetGameInstance() != GetGameInstance())
	{
		return;
	}

	HandleConnectionLost(FString::Printf(TEXT("%s: %s"), ETravelFailure::ToString(FailureType), *ErrorString));
}

void USavaSessionSubsystem::HandleConnectionLost(const FString& Reason)
{
	UE_LOG(LogSavaSession, Warning, TEXT("ホストとの接続が切れました: %s"), *Reason);

	//タイトル(Game Default Map)への移動はエンジンが行うので、ここでは部屋の情報を消すだけ
	DestroySessionThen([]() {});
	OnDisconnected.Broadcast(Reason);
}

// ---------------------------------------------------------------------------
// テスト用コンソールコマンド(UI ができるまでの確認用。` キーでコンソールを開いて打つ)
// ---------------------------------------------------------------------------

namespace SavaSession
{
	static USavaSessionSubsystem* GetSubsystem(UWorld* World)
	{
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<USavaSessionSubsystem>() : nullptr;
	}

	static FAutoConsoleCommandWithWorldAndArgs CreateRoomCommand(
		TEXT("Sava.CreateRoom"), TEXT("部屋を作る。例: Sava.CreateRoom MyRoom"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (USavaSessionSubsystem* Subsystem = GetSubsystem(World))
			{
				Subsystem->CreateRoom(Args.Num() > 0 ? Args[0] : FString());
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs FindRoomsCommand(
		TEXT("Sava.FindRooms"), TEXT("部屋を探して Output Log に一覧を出す"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (USavaSessionSubsystem* Subsystem = GetSubsystem(World))
			{
				Subsystem->FindRooms();
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs JoinRoomCommand(
		TEXT("Sava.JoinRoom"), TEXT("直前の Sava.FindRooms の番号の部屋に入る。例: Sava.JoinRoom 0"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (USavaSessionSubsystem* Subsystem = GetSubsystem(World))
			{
				Subsystem->JoinRoom(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0);
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs LeaveRoomCommand(
		TEXT("Sava.LeaveRoom"), TEXT("部屋を抜けてタイトルに戻る"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (USavaSessionSubsystem* Subsystem = GetSubsystem(World))
			{
				Subsystem->LeaveRoom();
			}
		}));
}

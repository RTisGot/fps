// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "SavaSessionSubsystem.generated.h"

//部屋一覧の 1 行分(UI に渡す)
USTRUCT(BlueprintType)
struct FSavaRoomInfo
{
	GENERATED_BODY()

	//JoinRoom に渡す番号(直前の FindRooms の結果の何番目か)
	UPROPERTY(BlueprintReadOnly, Category = "Sava|Session")
	int32 SearchIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Sava|Session")
	FString RoomName;

	UPROPERTY(BlueprintReadOnly, Category = "Sava|Session")
	FString HostName;

	UPROPERTY(BlueprintReadOnly, Category = "Sava|Session")
	int32 CurrentPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Sava|Session")
	int32 MaxPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Sava|Session")
	int32 PingMs = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSavaOnCreateRoomComplete, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSavaOnFindRoomsComplete, bool, bSuccess, const TArray<FSavaRoomInfo>&, Rooms);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSavaOnJoinRoomComplete, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSavaOnDisconnected, const FString&, Reason);

//部屋(オンラインセッション)を作る・探す・入る・抜ける。部屋に入った後のこと(チーム・Ready)は GameMode が受け持つ
//中身は Online Subsystem(今は Null = LAN、あとで EOS)。どれを使うかは DefaultEngine.ini の [OnlineSubsystem] で切り替える
//どの操作も非同期: 呼ぶとすぐ戻り、結果は On〜Complete で届く
UCLASS()
class SAVA_API USavaSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	//部屋を作る。成功したら Lobby マップを listen サーバーとして開く
	UFUNCTION(BlueprintCallable, Category = "Sava|Session")
	void CreateRoom(const FString& RoomName);

	//部屋を探す。結果は OnFindRoomsComplete
	UFUNCTION(BlueprintCallable, Category = "Sava|Session")
	void FindRooms();

	//直前の FindRooms の結果から部屋に入る。成功したらホストへ接続する
	UFUNCTION(BlueprintCallable, Category = "Sava|Session")
	void JoinRoom(int32 SearchIndex);

	//部屋を抜けてソロロビーに戻る。ホストが抜けると部屋ごと無くなる
	UFUNCTION(BlueprintCallable, Category = "Sava|Session")
	void LeaveRoom();

	//ソロロビーを開く(タイトルの「プレイ」から呼ぶ)
	UFUNCTION(BlueprintCallable, Category = "Sava|Session")
	void OpenSoloLobby();

	//ホストだけが呼ぶ。部屋に途中から入れるか(試合を始めるときに閉じ、検索にも出さなくする)
	void SetRoomJoinable(bool bJoinable);

	//ホストに追い出された(ASavaPlayerController::ClientWasKicked から)。この後の切断の理由をこれにする
	void NotifyKicked(const FString& Reason);

	//切断されてソロロビーへ戻ってきたときの理由を返して消す(なければ空。ソロロビーの画面を開いたときに呼ぶ)
	UFUNCTION(BlueprintCallable, Category = "Sava|Session")
	FString ConsumeDisconnectReason();

	//作成・検索・参加の途中か(UI でボタンを押せなくするのに使う)
	UFUNCTION(BlueprintPure, Category = "Sava|Session")
	bool IsBusy() const { return bBusy; }

	//部屋に入っているか(ホスト・参加者どちらでも)
	UFUNCTION(BlueprintPure, Category = "Sava|Session")
	bool IsInRoom() const;

	UPROPERTY(BlueprintAssignable, Category = "Sava|Session")
	FSavaOnCreateRoomComplete OnCreateRoomComplete;

	UPROPERTY(BlueprintAssignable, Category = "Sava|Session")
	FSavaOnFindRoomsComplete OnFindRoomsComplete;

	UPROPERTY(BlueprintAssignable, Category = "Sava|Session")
	FSavaOnJoinRoomComplete OnJoinRoomComplete;

	//ホストとの接続が切れた。この後マップが変わって今の画面は消えるので、理由を出すなら ConsumeDisconnectReason を使う
	UPROPERTY(BlueprintAssignable, Category = "Sava|Session")
	FSavaOnDisconnected OnDisconnected;

private:
	IOnlineSessionPtr GetSessionInterface() const;

	//前の部屋が残っていたら消してから Next を呼ぶ(1 プロセスで持てる部屋は 1 つだけ)
	void DestroySessionThen(TFunction<void()> Next);

	void StartCreateSession(FString RoomName);
	void StartJoinSession(FOnlineSessionSearchResult SearchResult);

	//Online Subsystem からの結果
	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleFindSessionsComplete(bool bWasSuccessful);
	void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);

	//エンジンからの切断・接続失敗の通知
	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);
	//Reason は画面に出す短い文、Details はログにだけ出すエンジンの詳細
	void HandleConnectionLost(const FString& Reason, const FString& Details);

	//切断後、エンジンがタイトル(Game Default Map)を開き終えたらソロロビーへ移し直す
	void HandlePostLoadMap(UWorld* LoadedWorld);

	//Null(LAN)で動いているか
	bool IsLANSubsystem() const;

	TSharedPtr<FOnlineSessionSearch> LastSearch;

	FDelegateHandle CreateSessionHandle;
	FDelegateHandle FindSessionsHandle;
	FDelegateHandle JoinSessionHandle;
	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;
	FDelegateHandle PostLoadMapHandle;

	bool bBusy = false;

	//切断され、ソロロビーへ戻す途中か
	bool bReturnToSoloLobbyPending = false;

	//最後に切断された理由(ConsumeDisconnectReason で消す)
	FString DisconnectReason;

	//追い出されたときの理由(この後の切断で DisconnectReason にする)
	FString PendingKickReason;
};

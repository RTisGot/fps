// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "savaGameMode.h"
#include "Lobby/SavaLobbyTypes.h"
#include "SavaLobbyGameMode.generated.h"

class ASavaLobbyGameState;
class ASavaPlayerController;
class ASavaPlayerState;

//ロビー(待合室)のルール。サーバーにだけある
//・部屋に入った人を人数の少ないチームへ入れ、ホストに印を付ける
//・ボタン・端末からの頼み(ASavaPlayerController の Server〜)を確かめて受け付ける
//  チーム切替・準備 OK は誰でも、ルーム設定・メンバー管理・ゲーム開始はホストだけ
//・ゲーム開始で、部屋を閉じてから、ルーム設定のモード・マップへ全員を連れて移る(Seamless Travel)
//どの頼みも、だめなときは false を返して理由を OutError に入れる(頼んだ人の画面に出す)
UCLASS()
class SAVA_API ASavaLobbyGameMode : public AsavaGameMode
{
	GENERATED_BODY()

public:
	ASavaLobbyGameMode();

	virtual void InitGameState() override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void InitSeamlessTravelPlayer(AController* NewController) override;

	//自分のチームを変える
	bool RequestJoinTeam(ASavaPlayerController* Requester, uint8 TeamId, FText& OutError);

	//準備 OK を切り替える
	bool RequestSetReady(ASavaPlayerController* Requester, bool bReady, FText& OutError);

	//ホストだけ: ルーム設定を変える(範囲外の値は直す)
	bool RequestUpdateRoomSettings(ASavaPlayerController* Requester, const FSavaRoomSettings& NewSettings, FText& OutError);

	//ホストだけ: 他の人のチームを変える
	bool RequestMovePlayerToTeam(ASavaPlayerController* Requester, ASavaPlayerState* Target, uint8 TeamId, FText& OutError);

	//ホストだけ: 他の人を部屋から追い出す
	bool RequestKickPlayer(ASavaPlayerController* Requester, ASavaPlayerState* Target, FText& OutError);

	//ホストだけ: 試合を始める
	bool RequestStartMatch(ASavaPlayerController* Requester, FText& OutError);

	//チームに入れるか(テストからも使う)
	static bool CanJoinTeam(const TArray<APlayerState*>& Players, const ASavaPlayerState* Player, uint8 TeamId, int32 NumTeams, int32 MaxPlayersPerTeam, FText& OutError);

	//試合マップへ移る URL(テストからも使う)。作れなければ空
	static FString BuildMatchTravelURL(const FSavaRoomSettings& RoomSettings, int32 NumPlayers);

protected:
	//「ゲーム開始」から試合マップへ移るまで待つ秒数(全員の画面に「開始します」を出す間)
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Lobby", meta = (ClampMin = "0", ForceUnits = "s"))
	float StartMatchDelay = 1.0f;

private:
	ASavaLobbyGameState* GetLobbyGameState() const;

	//ホストとして頼んでよいか(だめなら理由を入れる)
	bool CheckHost(const ASavaPlayerController* Requester, FText& OutError) const;

	//部屋にいる人の PlayerState か(他のマップの古いものや、なくなった人を弾く)
	bool IsLobbyPlayer(const ASavaPlayerState* PlayerState) const;

	bool ChangeTeam(ASavaPlayerState* Player, uint8 TeamId, FText& OutError);

	void MarkRoomHost(AController* Controller);

	//StartMatchDelay 後に呼ばれる
	void TravelToMatch();

	FTimerHandle StartMatchTimer;
};

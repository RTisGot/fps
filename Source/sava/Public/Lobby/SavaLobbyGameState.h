// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Lobby/SavaLobbyTypes.h"
#include "SavaLobbyGameState.generated.h"

class ASavaPlayerState;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSavaOnLobbyChanged);

//ロビー(待合室)の、全員が見られる情報(UI はここを読む)。値を変えるのはサーバーの ASavaLobbyGameMode だけ
//・ルーム設定(モード・マップ・ラウンド数・制限時間)
//・試合を始めているところか
//プレイヤーごとのチーム・準備 OK・ホストは ASavaPlayerState にある(PlayerArray から読む)
UCLASS()
class SAVA_API ASavaLobbyGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void AddPlayerState(APlayerState* PlayerState) override;
	virtual void RemovePlayerState(APlayerState* PlayerState) override;

	UFUNCTION(BlueprintPure, Category = "Sava|Lobby")
	const FSavaRoomSettings& GetRoomSettings() const { return RoomSettings; }

	//ホストが「ゲーム開始」を押し、試合マップへ移るところか
	UFUNCTION(BlueprintPure, Category = "Sava|Lobby")
	bool IsMatchStarting() const { return bMatchStarting; }

	UFUNCTION(BlueprintPure, Category = "Sava|Lobby")
	int32 GetNumTeams() const { return NumTeams; }

	//今のルーム設定がチームに分かれて戦うか(個人戦なら false。チームの表示を隠すのに使う)
	UFUNCTION(BlueprintPure, Category = "Sava|Lobby")
	bool IsTeamMatch() const;

	UFUNCTION(BlueprintPure, Category = "Sava|Lobby")
	int32 GetMaxPlayersPerTeam() const { return MaxPlayersPerTeam; }

	//部屋にいる全員(ホストを含む)
	UFUNCTION(BlueprintPure, Category = "Sava|Lobby")
	TArray<ASavaPlayerState*> GetLobbyPlayers() const;

	//そのチームの人
	UFUNCTION(BlueprintPure, Category = "Sava|Lobby")
	TArray<ASavaPlayerState*> GetTeamPlayers(uint8 TeamId) const;

	//今の状態で試合を始められるか(だめなら理由。開始ボタンの表示に使う。最終的な判断はサーバー)
	UFUNCTION(BlueprintPure, Category = "Sava|Lobby")
	bool CanStartMatch(FText& OutReason) const;

	//ルーム設定・人の出入り・誰かのチーム/準備 OK が変わった(ロビーの画面はこれ 1 つを見て描き直せばよい)
	UPROPERTY(BlueprintAssignable, Category = "Sava|Lobby")
	FSavaOnLobbyChanged OnLobbyChanged;

	//OnLobbyChanged を呼ぶ(ASavaPlayerState からも呼ぶ)
	void NotifyLobbyChanged();

	//以下はサーバー(ASavaLobbyGameMode)だけが呼ぶ
	void InitTeams(int32 InNumTeams, int32 InMaxPlayersPerTeam);
	void SetRoomSettings(const FSavaRoomSettings& NewSettings);
	void SetMatchStarting(bool bStarting);

	//チームの人数(TeamId のチームにいる人。Exclude は数えない)
	static int32 CountTeamPlayers(const TArray<APlayerState*>& Players, uint8 TeamId, const APlayerState* Exclude = nullptr);

private:
	UFUNCTION()
	void OnRep_LobbyInfo();

	UPROPERTY(ReplicatedUsing = OnRep_LobbyInfo)
	FSavaRoomSettings RoomSettings;

	UPROPERTY(ReplicatedUsing = OnRep_LobbyInfo)
	bool bMatchStarting = false;

	UPROPERTY(Replicated)
	int32 NumTeams = 2;

	UPROPERTY(Replicated)
	int32 MaxPlayersPerTeam = 3;
};

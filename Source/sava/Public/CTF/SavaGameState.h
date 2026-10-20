// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "SavaGameState.generated.h"

class ASavaPlayerState;

//ラウンドの段階
UENUM(BlueprintType)
enum class ESavaRoundPhase : uint8
{
	//試合の開始を待っている(人数が揃うまで)
	WaitingToStart,
	//ラウンド開始前のカウントダウン(動けない)
	PreRound,
	//ラウンド中
	InRound,
	//時間切れ後のデスマッチ(全員リスポーン不可・旗なし)
	Deathmatch,
	//ラウンドの結果表示
	RoundEnd,
	//試合終了
	MatchEnd,
};

//チームごとの、試合・ラウンドの状態
USTRUCT(BlueprintType)
struct FSavaTeamRoundState
{
	GENERATED_BODY()

	//取ったラウンド数
	UPROPERTY(BlueprintReadOnly, Category = "Sava|Round")
	int32 RoundWins = 0;

	//このラウンド中にリスポーンできなくなったか(自分の旗を持ち帰られた・デスマッチ)
	UPROPERTY(BlueprintReadOnly, Category = "Sava|Round")
	bool bRespawnDisabled = false;
};

//全員が見られる試合の情報(UI はここを読む)。値を変えるのはサーバーの ASavaCTFGameMode だけ
UCLASS()
class SAVA_API ASavaGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	//勝者なし(引き分け・未決定)
	static constexpr uint8 NoTeam = 255;

	ASavaGameState();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Sava|Round")
	ESavaRoundPhase GetRoundPhase() const { return RoundPhase; }

	//ラウンドが進行中か(旗を拾える・リスポーンの判定をする)
	UFUNCTION(BlueprintPure, Category = "Sava|Round")
	bool IsRoundInProgress() const { return RoundPhase == ESavaRoundPhase::InRound || RoundPhase == ESavaRoundPhase::Deathmatch; }

	//今の段階の残り秒数(時間制限のない段階では 0)
	UFUNCTION(BlueprintPure, Category = "Sava|Round")
	float GetPhaseTimeRemaining() const;

	//何ラウンド目か(1 から)
	UFUNCTION(BlueprintPure, Category = "Sava|Round")
	int32 GetRoundNumber() const { return RoundNumber; }

	UFUNCTION(BlueprintPure, Category = "Sava|Round")
	int32 GetNumTeams() const { return TeamStates.Num(); }

	UFUNCTION(BlueprintPure, Category = "Sava|Round")
	int32 GetTeamRoundWins(uint8 TeamId) const;

	UFUNCTION(BlueprintPure, Category = "Sava|Round")
	bool CanTeamRespawn(uint8 TeamId) const;

	//直前のラウンドの勝者(引き分けなら 255)
	UFUNCTION(BlueprintPure, Category = "Sava|Round")
	uint8 GetLastRoundWinner() const { return LastRoundWinner; }

	//試合の勝者(決まっていなければ 255)
	UFUNCTION(BlueprintPure, Category = "Sava|Round")
	uint8 GetMatchWinner() const { return MatchWinner; }

	// デスマッチの勝者名を取得（同率1位を含む）
	UFUNCTION(BlueprintPure, Category = "Sava|Deathmatch")
	const TArray<FString>& GetDeathmatchWinnerNames() const
	{
		return DeathmatchWinnerNames;
	}

	// デスマッチの最高キル数を取得
	UFUNCTION(BlueprintPure, Category = "Sava|Deathmatch")
	int32 GetDeathmatchWinningKills() const
	{
		return DeathmatchWinningKills;
	}

	//---- デスマッチ(ASavaDeathmatchGameMode が値を入れる。CTF では使わない)

	//デスマッチの試合か
	UFUNCTION(BlueprintPure, Category = "Sava|Deathmatch")
	bool IsDeathmatch() const { return bIsDeathmatch; }

	//デスマッチのチーム戦か(個人戦なら false)
	UFUNCTION(BlueprintPure, Category = "Sava|Deathmatch")
	bool IsDeathmatchTeamMatch() const { return bDeathmatchTeamMatch; }

	//勝利に必要なキル数(チーム戦ならチームの合計)
	UFUNCTION(BlueprintPure, Category = "Sava|Deathmatch")
	int32 GetDeathmatchKillLimit() const { return DeathmatchKillLimit; }

	//チームの合計キル数(全員の PlayerState から数えるので、クライアントでも使える)
	UFUNCTION(BlueprintPure, Category = "Sava|Deathmatch")
	int32 GetTeamKills(uint8 TeamId) const;

	//全員をキル数の多い順に並べたもの(同じならデスの少ない順)。リザルト・スコア表示用
	UFUNCTION(BlueprintPure, Category = "Sava|Deathmatch")
	TArray<ASavaPlayerState*> GetPlayersByKills() const;

	//勝者(個人戦: 1 位の人。同率なら全員 / チーム戦: 勝ったチームの全員。引き分けなら並んだチームの全員)
	//チーム戦で勝ったチームの番号は GetMatchWinner(引き分けなら 255)
	UFUNCTION(BlueprintPure, Category = "Sava|Deathmatch")
	TArray<ASavaPlayerState*> GetDeathmatchWinners() const;

	//引き分けか(個人戦: 1 位が複数 / チーム戦: 勝ったチームがない)。試合が終わってから使う
	UFUNCTION(BlueprintPure, Category = "Sava|Deathmatch")
	bool IsDeathmatchDraw() const;

	//以下はサーバー(ASavaDeathmatchGameMode)だけが呼ぶ
	void SetDeathmatchRules(bool bTeamMatch, int32 KillLimit);
	//WinnerTeamId はチーム戦で勝ったチーム(個人戦・引き分けなら NoTeam)
	void SetDeathmatchResults(const TArray<ASavaPlayerState*>& Winners, int32 WinningKills, uint8 WinnerTeamId);

	//以下はサーバー(ASavaCTFGameMode)だけが呼ぶ
	void InitTeams(int32 NumTeams);
	void SetRoundPhase(ESavaRoundPhase NewPhase, float Duration);
	void BeginNewRound();
	void SetTeamRespawnDisabled(uint8 TeamId, bool bDisabled);
	void AddRoundWin(uint8 TeamId);
	void SetMatchWinner(uint8 TeamId) { MatchWinner = TeamId; }

protected:
	//画面左上に試合の状態を表示する(UI ができるまでの動作確認用)
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Debug")
	bool bShowDebugText = true;

private:
	UPROPERTY(Replicated)
	ESavaRoundPhase RoundPhase = ESavaRoundPhase::WaitingToStart;

	//今の段階が終わるサーバー時刻(GetServerWorldTimeSeconds 基準)。0 なら時間制限なし
	UPROPERTY(Replicated)
	float PhaseEndServerTime = 0.0f;

	UPROPERTY(Replicated)
	int32 RoundNumber = 0;

	UPROPERTY(Replicated)
	TArray<FSavaTeamRoundState> TeamStates;

	UPROPERTY(Replicated)
	uint8 LastRoundWinner = NoTeam;

	UPROPERTY(Replicated)
	uint8 MatchWinner = NoTeam;

	// デスマッチの勝者名（同率1位を含む）
	UPROPERTY(Replicated)
	TArray<FString> DeathmatchWinnerNames;

	// 勝者のキル数
	UPROPERTY(Replicated)
	int32 DeathmatchWinningKills = 0;

	//勝者(GetDeathmatchWinners を参照)
	UPROPERTY(Replicated)
	TArray<TObjectPtr<ASavaPlayerState>> DeathmatchWinners;

	UPROPERTY(Replicated)
	bool bIsDeathmatch = false;

	UPROPERTY(Replicated)
	bool bDeathmatchTeamMatch = false;

	UPROPERTY(Replicated)
	int32 DeathmatchKillLimit = 0;

	void DrawDebugText() const;
	void DrawDeathmatchDebugText() const;
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "savaGameMode.h"
#include "SavaCTFGameMode.generated.h"

class ASavaFlag;
class ASavaFlagBase;
class ASavaGameState;
class ASavaPlayerState;

//CTF(ラウンド制)。仕様は Docs/CTF.md
//・相手の旗を持ち帰ると、相手はそのラウンド中リスポーン不可
//・リスポーンできない相手チームを全滅させたらラウンドの勝ち。RoundsToWin ラウンド先取で試合の勝ち
//・時間切れ: どちらの旗も持ち帰られていなければデスマッチ、片方だけなら旗が残っているチームの勝ち
//レベルには ASavaFlagBase(各チーム 1 個)と ASavaTeamPlayerStart(各チーム 3 個以上)を置く
UCLASS()
class SAVA_API ASavaCTFGameMode : public AsavaGameMode
{
	GENERATED_BODY()

public:
	ASavaCTFGameMode();

	//ロビーのルーム設定を URL のオプションから読む(Rounds・TimeLimit・Players。SavaMatchOptions を参照)
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void InitGameState() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	//待合室から Seamless Travel で移ってきた人(PostLogin は呼ばれない)
	virtual void HandleSeamlessTravelPlayer(AController*& C) override;
	virtual void Logout(AController* Exiting) override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual bool PlayerCanRestart_Implementation(APlayerController* Player) override;
	virtual bool ShouldSpawnAtStartSpot(AController* Player) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual void NotifyPlayerDied(AController* DeadController) override;

	//試合中はロードアウトを変えられない。最初の 1 回(待合室を通らずに入ったときの分)だけ受け付ける
	virtual bool CanChangeLoadout(const ASavaPlayerState* PlayerState) const override;

	//以下は旗(ASavaFlag)が呼ぶ
	ASavaFlagBase* FindFlagBase(uint8 TeamId) const;
	//そのチームが今、敵の旗を持ち帰れるか(自分の旗が台にある、または持ち帰られて消えている)
	bool CanTeamCapture(uint8 TeamId) const;
	bool IsRoundInProgress() const;
	void NotifyFlagCaptured(ASavaFlag* CapturedFlag, APawn* Carrier);

protected:
	virtual void RespawnPlayer(TWeakObjectPtr<AController> Controller) override;

	//試合に勝つのに必要なラウンド数
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Round", meta = (ClampMin = "1"))
	int32 RoundsToWin = 2;

	//何人集まったら試合を始めるか(テスト中は 1、本番は 6)
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Round", meta = (ClampMin = "1"))
	int32 MinPlayersToStart = 1;

	//待合室から移ってくる人が全員そろうまで待つ上限。過ぎたら MinPlayersToStart 人いれば始める
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Round", meta = (ClampMin = "0", ForceUnits = "s"))
	float WaitForPlayersTimeout = 30.0f;

	//ラウンド開始前のカウントダウン(動けない)
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Round", meta = (ClampMin = "0", ForceUnits = "s"))
	float PreRoundDuration = 3.0f;

	//1 ラウンドの制限時間
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Round", meta = (ClampMin = "1", ForceUnits = "s"))
	float RoundTimeLimit = 180.0f;

	//ラウンドの結果表示
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Round", meta = (ClampMin = "0", ForceUnits = "s"))
	float RoundEndDuration = 5.0f;

private:
	ASavaGameState* GetSavaGameState() const;

	//人がそろったら最初のラウンドを始める(入ってくるたびに呼ぶ)
	void NotifyPlayerArrived(AController* Controller);
	void TryStartFirstRound(bool bTimedOut);
	void OnWaitForPlayersTimeout();

	//待合室から移ってくる人数(URL の Players。直接このマップを開いたときは 0)
	int32 ExpectedPlayers = 0;

	//このマップに入り終えた人
	TSet<TWeakObjectPtr<AController>> ArrivedPlayers;

	FTimerHandle WaitForPlayersTimer;

	//ラウンドの流れ: StartRound(カウントダウン) → BeginRoundPlay → (OnRoundTimeUp → StartDeathmatch) → EndRound → StartRound …
	void StartRound();
	void BeginRoundPlay();
	void OnRoundTimeUp();
	void StartDeathmatch();
	void EndRound(uint8 WinnerTeamId);

	//全滅したチームを調べて、残りが 1 チーム以下ならラウンドを終える
	void CheckRoundEnd();
	//リスポーンできず、生きている人もいないか
	bool IsTeamEliminated(uint8 TeamId) const;

	//ラウンドのやり直し: 死体・設置物を消し、全員を出し直す
	void CleanUpWorldForNewRound();
	void RespawnAllPlayersForNewRound();

	void ClearRespawnTimers();

	//リスポーン待ちのタイマー(ラウンドのやり直しで取り消す)
	TMap<TWeakObjectPtr<AController>, FTimerHandle> RespawnTimers;

	//カウントダウン・制限時間・結果表示のタイマー
	FTimerHandle PhaseTimer;
};

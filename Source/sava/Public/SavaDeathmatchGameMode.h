// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "savaGameMode.h"
#include "SavaDeathmatchGameMode.generated.h"

class APlayerState;
class ASavaGameState;
class ASavaPlayerState;

//デスマッチ。仕様は Docs/Deathmatch.md
//倒されても RespawnDelay 秒後にリスポーンする。個人戦とチーム戦はルーム設定で選ぶ(URL の Teams)
//個人戦: 全員チームなし(全員が敵)。KillLimit キルに最初に届いた人の勝ち
//チーム戦: 待合室のチームのまま。チームの合計キルが KillLimit に最初に届いたチームの勝ち
//・時間切れならキル数(チーム戦ならチームの合計)が一番多い方の勝ち(同数なら引き分けで全員を勝者として記録する)
//・終わったら結果画面を出し、ReturnToLobbyDelay 秒後に部屋のロビーへ戻る
//試合の進み方(人がそろうまで待つ → カウントダウン → 試合 → 終了)は ASavaCTFGameMode と同じ
UCLASS()
class SAVA_API ASavaDeathmatchGameMode : public AsavaGameMode
{
	GENERATED_BODY()

public:
	ASavaDeathmatchGameMode();

	//ロビーのルーム設定を URL のオプションから読む(TimeLimit・Kills・Teams・Players。SavaMatchOptions を参照)
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	//待合室から Seamless Travel で移ってきた人(PostLogin は呼ばれない)
	virtual void HandleSeamlessTravelPlayer(AController*& C) override;
	virtual void Logout(AController* Exiting) override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual bool ShouldSpawnAtStartSpot(AController* Player) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	//試合中はロードアウトを変えられない。最初の 1 回(待合室を通らずに入ったときの分)だけ受け付ける
	virtual bool CanChangeLoadout(const ASavaPlayerState* PlayerState) const override;

	//キル数が増えたときに ASavaPlayerState が呼ぶ。KillLimit に届いたら試合を終える
	void CheckKillLimit(ASavaPlayerState* Killer);

	//勝者を決める(時間切れのとき)。個人戦: キル数が一番多い人(同数なら全員)
	//チーム戦: 合計キルが一番多いチームの全員(同数なら並んだチームの全員で、OutWinnerTeamId は NoTeam)
	static void DecideWinners(const TArray<APlayerState*>& Players, bool bTeamMatch, int32 NumTeams,
		TArray<ASavaPlayerState*>& OutWinners, int32& OutWinningKills, uint8& OutWinnerTeamId);

	//スポーン地点の候補を選ぶ。使用中(Occupied の近く)を除き、一番近い敵が一番遠い地点と、それに近いくらい安全な地点の番号を返す
	//敵がいなければ空いている地点すべて。全部使用中なら全部
	static TArray<int32> FindSafestStarts(const TArray<FVector>& StartLocations, const TArray<FVector>& EnemyLocations, const TArray<FVector>& OccupiedLocations);

protected:
	virtual void RespawnPlayer(TWeakObjectPtr<AController> Controller) override;

	//試合の制限時間(ルーム設定の TimeLimit があればそちらを使う)
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Deathmatch", meta = (ClampMin = "1", ForceUnits = "s"))
	float MatchDuration = 300.0f;

	//勝利に必要なキル数(チーム戦ならチームの合計。ルーム設定の Kills があればそちらを使う)
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Deathmatch", meta = (ClampMin = "1"))
	int32 KillLimit = 10;

	//何人集まったら試合を始めるか(待合室を通らずにこのマップを開いたとき)
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Deathmatch", meta = (ClampMin = "1"))
	int32 MinPlayersToStart = 1;

	//待合室から移ってくる人が全員そろうまで待つ上限。過ぎたら MinPlayersToStart 人いれば始める
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Deathmatch", meta = (ClampMin = "0", ForceUnits = "s"))
	float WaitForPlayersTimeout = 30.0f;

	//試合開始前のカウントダウン(動けない)
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Deathmatch", meta = (ClampMin = "0", ForceUnits = "s"))
	float PreMatchDuration = 3.0f;

private:
	ASavaGameState* GetSavaGameState() const;

	//人がそろったら試合を始める(入ってくるたびに呼ぶ)
	void NotifyPlayerArrived(AController* Controller);
	void TryStartMatch(bool bTimedOut);
	void OnWaitForPlayersTimeout();

	//試合の流れ: StartCountdown → BeginMatchPlay → (KillLimit 到達 / OnMatchTimeUp) → FinishMatch
	void StartCountdown();
	void BeginMatchPlay();
	void OnMatchTimeUp();
	//WinnerTeamId はチーム戦で勝ったチーム(個人戦・引き分けなら NoTeam)
	void FinishMatch(const TArray<ASavaPlayerState*>& Winners, int32 WinningKills, uint8 WinnerTeamId);

	//チームの全員
	TArray<ASavaPlayerState*> GetTeamPlayers(uint8 TeamId) const;

	//待合室から移ってくる人数(URL の Players。直接このマップを開いたときは 0)
	int32 ExpectedPlayers = 0;

	//このマップに入り終えた人
	TSet<TWeakObjectPtr<AController>> ArrivedPlayers;

	FTimerHandle WaitForPlayersTimer;

	//カウントダウン・制限時間のタイマー
	FTimerHandle PhaseTimer;
};

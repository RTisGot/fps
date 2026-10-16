// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "savaGameMode.generated.h"

UCLASS(minimalapi)
class AsavaGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AsavaGameMode();

	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;

	//Seamless Travel(待合室 → 試合)で移ってきた人。チームは PlayerState ごと引き継がれるので、決まっていない人だけ振り分ける
	virtual void InitSeamlessTravelPlayer(AController* NewController) override;

	//サーバーで呼ぶ。死んだプレイヤーを RespawnDelay 秒後に復活させる
	virtual void NotifyPlayerDied(AController* DeadController);

	//サーバーで呼ぶ。このプレイヤーから届いたロードアウトを受け付けるか(ソロロビー・待合室ではいつでも受け付ける)
	virtual bool CanChangeLoadout(const class ASavaPlayerState* PlayerState) const { return true; }

protected:
	//チーム数(3v3 なら 2)。参加者は人数の少ないチームへ自動で振り分ける
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Team", meta = (ClampMin = "1"))
	int32 NumTeams = 2;

	//1 チームの人数(3v3 なら 3)。NumTeams × この値が参加できる人数の上限
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Team", meta = (ClampMin = "1"))
	int32 MaxPlayersPerTeam = 3;

	//死んでからリスポーンするまでの秒数
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Respawn", meta = (ClampMin = "0", ForceUnits = "s"))
	float RespawnDelay = 4.0f;

	//タイマー満了で呼ばれる。古い Pawn を片付けて新しい Pawn を出す
	virtual void RespawnPlayer(TWeakObjectPtr<AController> Controller);

	//人数の少ないチームに入れる
	void AssignTeam(AController* NewPlayer);

	//試合が終わってから部屋のロビーへ戻るまでの秒数(結果を見る間)
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Match", meta = (ClampMin = "0", ForceUnits = "s"))
	float ReturnToLobbyDelay = 10.0f;

	//試合が終わったら呼ぶ。ReturnToLobbyDelay 秒後に全員を連れて部屋のロビーへ戻る
	void ScheduleReturnToLobby();

private:
	void ReturnToLobby();

	FTimerHandle ReturnToLobbyTimer;
};
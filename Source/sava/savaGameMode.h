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

private:
	void AssignTeam(APlayerController* NewPlayer);
};
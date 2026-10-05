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

	//サーバーで呼ぶ。死んだプレイヤーを RespawnDelay 秒後に復活させる
	void NotifyPlayerDied(AController* DeadController);

protected:
	//チーム数(3v3 なら 2)。参加者は人数の少ないチームへ自動で振り分ける
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Team", meta = (ClampMin = "1"))
	int32 NumTeams = 2;

	//死んでからリスポーンするまでの秒数
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Respawn", meta = (ClampMin = "0", ForceUnits = "s"))
	float RespawnDelay = 4.0f;

private:
	//タイマー満了で呼ばれる。古い Pawn を片付けて新しい Pawn を出す
	void RespawnPlayer(TWeakObjectPtr<AController> Controller);
};
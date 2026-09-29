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

protected:
	//チーム数(3v3 なら 2)。参加者は人数の少ないチームへ自動で振り分ける
	UPROPERTY(EditDefaultsOnly, Category = "Sava|Team", meta = (ClampMin = "1"))
	int32 NumTeams = 2;
};




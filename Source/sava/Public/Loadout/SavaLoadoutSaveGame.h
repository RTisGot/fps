// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Loadout/SavaLoadoutTypes.h"
#include "SavaLoadoutSaveGame.generated.h"

//ディスクに保存するロードアウト(Saved/SaveGames/Loadout.sav)
UCLASS()
class SAVA_API USavaLoadoutSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FSavaLoadout Loadout;
};

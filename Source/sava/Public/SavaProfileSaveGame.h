#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "SavaProfileSaveGame.generated.h"

//ディスクに保存するプレイヤーの情報(Saved/SaveGames/Profile.sav)
UCLASS()
class SAVA_API USavaProfileSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY()
    FString PlayerName;
};

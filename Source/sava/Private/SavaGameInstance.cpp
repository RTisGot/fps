#include "SavaGameInstance.h"

void USavaGameInstance::SetPlayerName(const FString& NewPlayerName)
{
    if (NewPlayerName.IsEmpty())
    {
        PlayerName = TEXT("Player");
        return;
    }

    PlayerName = NewPlayerName;
}

const FString& USavaGameInstance::GetPlayerName() const
{
    return PlayerName;
}
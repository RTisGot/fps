#include "SavaLocalPlayer.h"
#include "SavaGameInstance.h"

FString USavaLocalPlayer::GetNickname() const
{
    if (const USavaGameInstance* GameInstance = Cast<USavaGameInstance>(GetGameInstance()))
    {
        return GameInstance->GetPlayerName();
    }

    return Super::GetNickname();
}

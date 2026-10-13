#pragma once

#include "CoreMinimal.h"
#include "Engine/LocalPlayer.h"
#include "SavaLocalPlayer.generated.h"

//この PC で遊んでいるプレイヤー
//サーバーに入るとき、エンジンは GetNickname の名前を「?Name=〇〇」として送り、それが PlayerState の名前になる
//既定では Online Subsystem の名前(Null なら PC 名)になるので、USavaGameInstance で設定した名前を返す
UCLASS()
class SAVA_API USavaLocalPlayer : public ULocalPlayer
{
    GENERATED_BODY()

public:
    virtual FString GetNickname() const override;
};

#pragma once

#include "CoreMinimal.h"
#include "savaGameMode.h"
#include "Net/UnrealNetwork.h"
#include "SavaDeathmatchGameMode.generated.h"

class ASavaPlayerState;

/**
 * @brief デスマッチ専用のGameMode
 */
UCLASS()
class SAVA_API ASavaDeathmatchGameMode : public AsavaGameMode
{
    GENERATED_BODY()

public:
    ASavaDeathmatchGameMode();

    // キルが発生したときの勝利判定
    void CheckKillLimit(ASavaPlayerState* Killer);

    // キル上限に到達したプレイヤーを勝者にする
    void FinishMatchWithWinner(ASavaPlayerState* Winner);

protected:
    // 試合時間（秒）
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Deathmatch")
    float MatchDuration = 300.0f;

    // 勝利に必要なキル数
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Deathmatch",
        meta = (ClampMin = "1"))
    int32 KillLimit = 30;

    // 試合開始時の処理
    virtual void BeginPlay() override;

private:
    FTimerHandle MatchTimerHandle;

    // 試合時間を更新する
    void UpdateMatchTime();

    // 試合時間終了時の処理
    void FinishMatch();
};

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "SavaGameInstance.generated.h"

/**
 * @brief マップをまたいで持つ、自分の PC だけの情報を管理する GameInstance。
 * プレイヤー名はディスクに保存し、部屋に入るときにサーバーへ送る(USavaLocalPlayer)
 */
UCLASS()
class SAVA_API USavaGameInstance : public UGameInstance
{
    GENERATED_BODY()

public:
    virtual void Init() override;

    /** プレイヤー名を設定して保存する(使えない文字は消し、長すぎる分は切る。空なら "Player") */
    UFUNCTION(BlueprintCallable, Category = "Sava|Player")
    void SetPlayerName(const FString& NewPlayerName);

    /** プレイヤー名を取得する */
    UFUNCTION(BlueprintPure, Category = "Sava|Player")
    const FString& GetPlayerName() const;

    /** プレイヤー名の最大文字数(エンジンがサーバー側でこの長さに切るため) */
    static constexpr int32 MaxPlayerNameLength = 20;

private:
    /** 使えない文字を消し、長さをそろえた名前を返す */
    static FString SanitizePlayerName(const FString& InName);

    /** 設定したプレイヤー名 */
    UPROPERTY()
    FString PlayerName = TEXT("Player");
};

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "SavaGameInstance.generated.h"
/**
 * @brief タイトル画面からゲームへ引き継ぐ情報を管理するGameInstance。
 */
UCLASS()
class SAVA_API USavaGameInstance : public UGameInstance
{
    GENERATED_BODY()

public:
    /** プレイヤー名を設定する */
    UFUNCTION(BlueprintCallable, Category = "Sava|Player")
    void SetPlayerName(const FString& NewPlayerName);

    /** プレイヤー名を取得する */
    UFUNCTION(BlueprintPure, Category = "Sava|Player")
    const FString& GetPlayerName() const;

private:
    /** タイトル画面で設定したプレイヤー名 */
    UPROPERTY()
    FString PlayerName = TEXT("Player");
};
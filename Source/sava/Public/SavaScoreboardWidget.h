#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SavaScoreboardWidget.generated.h"

class UVerticalBox;

/**
 * @brief Tabキーで表示するスコアボード。
 *        全プレイヤーの名前とキル数を表示する。
 */
UCLASS()
class SAVA_API USavaScoreboardWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** 全プレイヤーのキル数を再取得して表示する */
    UFUNCTION(BlueprintCallable, Category = "Sava|Scoreboard")
    void RefreshScoreboard();

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(
        const FGeometry& MyGeometry,
        float InDeltaTime) override;

private:
    /** プレイヤー一覧を配置するVerticalBox */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UVerticalBox> PlayerList;
    
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UVerticalBox> KillList;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UVerticalBox> DeathList;

    /** 表示中のキル数を更新する間隔 */
    float RefreshTimer = 0.0f;
};
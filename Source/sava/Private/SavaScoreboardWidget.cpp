#include "SavaScoreboardWidget.h"

#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Player/SavaPlayerState.h"

#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"

void USavaScoreboardWidget::NativeConstruct()
{
    Super::NativeConstruct();

    RefreshScoreboard();
}

void USavaScoreboardWidget::NativeTick(
    const FGeometry& MyGeometry,
    float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    RefreshTimer += InDeltaTime;

    if (RefreshTimer < 0.2f)
    {
        return;
    }

    RefreshTimer = 0.0f;
    RefreshScoreboard();
}

void USavaScoreboardWidget::RefreshScoreboard()
{
	// ---- PlayerList, KillList, DeathList が有効か確認 ----
    if (!PlayerList)
    {
        return;
    }

    if (!KillList)
    {
        return;
    }

    if (!DeathList)
    {
        return;
    }

    //-------------------------------------------------------

	// すべてのプレイヤーの情報を取得するために GameState を取得する
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    AGameStateBase* GameState = World->GetGameState();
    if (!GameState)
    {
        return;
    }

	// ----Listをクリアしてから再構築する----
    PlayerList->ClearChildren();
    KillList->ClearChildren();
    DeathList->ClearChildren();

    //---------------------------------------

	// すべてのプレイヤーの情報を取得して表示する

    for (APlayerState* PlayerState : GameState->PlayerArray)
    {
        ASavaPlayerState* SavaPlayerState =
            Cast<ASavaPlayerState>(PlayerState);

        if (!SavaPlayerState)
        {
            continue;
        }

		// プレイヤー名、キル数、デス数を取得する
        const FString PlayerName = SavaPlayerState->GetPlayerName();
        const int32 KillCount = SavaPlayerState->GetKillCount();
        const int32 DeathCount = SavaPlayerState->GetDeathCount();

		// 新しい UTextBlock を作成して、プレイヤー名、キル数、デス数を設定する
        UTextBlock* PlayerText = NewObject<UTextBlock>(this);
        UTextBlock* KillText = NewObject<UTextBlock>(this);
        UTextBlock* DeathText = NewObject<UTextBlock>(this);

        if (!PlayerText || !KillText || !DeathText)
        {
            continue;
        }

		// プレイヤー名、キル数、デス数を UTextBlock に設定する
        PlayerText->SetText(
            FText::FromString(
                FString::Printf(
                    TEXT("%s"),
                    *PlayerName)));

        KillText->SetText(
            FText::FromString(
                FString::Printf(
                    TEXT("%d KILLS"),
                    KillCount)));

        DeathText->SetText(
            FText::FromString(
                FString::Printf(
                    TEXT("%d Death"),
                    DeathCount)));

		// UVerticalBox に UTextBlock を追加する
        PlayerList->AddChild(PlayerText);
        KillList->AddChild(KillText);
        DeathList->AddChild(DeathText);
    }
}

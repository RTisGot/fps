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
    if (!PlayerList)
    {
        return;
    }

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

    PlayerList->ClearChildren();

    for (APlayerState* PlayerState : GameState->PlayerArray)
    {
        ASavaPlayerState* SavaPlayerState =
            Cast<ASavaPlayerState>(PlayerState);

        if (!SavaPlayerState)
        {
            continue;
        }

        const FString PlayerName = SavaPlayerState->GetPlayerName();
        const int32 KillCount = SavaPlayerState->GetKillCount();

        UTextBlock* PlayerText = NewObject<UTextBlock>(this);
        if (!PlayerText)
        {
            continue;
        }

        PlayerText->SetText(
            FText::FromString(
                FString::Printf(
                    TEXT("%s        %d KILLS"),
                    *PlayerName,
                    KillCount)));

        PlayerList->AddChild(PlayerText);
    }
}

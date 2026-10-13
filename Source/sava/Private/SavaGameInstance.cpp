#include "SavaGameInstance.h"
#include "SavaProfileSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/IConsoleManager.h"

namespace SavaProfile
{
    const FString SaveSlotName(TEXT("Profile"));
}

void USavaGameInstance::Init()
{
    Super::Init();

    if (const USavaProfileSaveGame* SaveGame = Cast<USavaProfileSaveGame>(UGameplayStatics::LoadGameFromSlot(SavaProfile::SaveSlotName, 0)))
    {
        PlayerName = SanitizePlayerName(SaveGame->PlayerName);
    }
}

void USavaGameInstance::SetPlayerName(const FString& NewPlayerName)
{
    PlayerName = SanitizePlayerName(NewPlayerName);

    USavaProfileSaveGame* SaveGame = Cast<USavaProfileSaveGame>(UGameplayStatics::CreateSaveGameObject(USavaProfileSaveGame::StaticClass()));
    SaveGame->PlayerName = PlayerName;
    UGameplayStatics::SaveGameToSlot(SaveGame, SavaProfile::SaveSlotName, 0);
}

const FString& USavaGameInstance::GetPlayerName() const
{
    return PlayerName;
}

FString USavaGameInstance::SanitizePlayerName(const FString& InName)
{
    //名前はサーバーへ「?Name=〇〇」の形で送るので、区切りに使われる文字は使えない
    FString Name = InName;
    for (const TCHAR Forbidden : { TEXT('?'), TEXT('='), TEXT('#'), TEXT('&'), TEXT('"') })
    {
        Name.ReplaceCharInline(Forbidden, TEXT(' '));
    }
    Name.TrimStartAndEndInline();
    Name.LeftInline(MaxPlayerNameLength);

    return Name.IsEmpty() ? FString(TEXT("Player")) : Name;
}

//テスト用コンソールコマンド(名前を入れる画面ができるまでの確認用)
static FAutoConsoleCommandWithWorldAndArgs GSavaSetPlayerNameCommand(
    TEXT("Sava.SetPlayerName"), TEXT("プレイヤー名を変える。例: Sava.SetPlayerName Taro"),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
    {
        if (USavaGameInstance* GameInstance = World ? Cast<USavaGameInstance>(World->GetGameInstance()) : nullptr)
        {
            GameInstance->SetPlayerName(FString::Join(Args, TEXT(" ")));
            UE_LOG(LogTemp, Log, TEXT("プレイヤー名: %s"), *GameInstance->GetPlayerName());
        }
    }));

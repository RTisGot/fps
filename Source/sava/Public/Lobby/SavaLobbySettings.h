// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Lobby/SavaLobbyTypes.h"
#include "SavaLobbySettings.generated.h"

class AGameModeBase;
class APlayerState;

//試合マップの候補
USTRUCT(BlueprintType)
struct FSavaMatchMapOption
{
	GENERATED_BODY()

	//ルーム設定の画面に出す名前
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sava|Lobby")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sava|Lobby")
	TSoftObjectPtr<UWorld> Map;
};

//モード(CTF・デスマッチなど)の候補
USTRUCT(BlueprintType)
struct FSavaMatchModeOption
{
	GENERATED_BODY()

	//ルーム設定の画面に出す名前
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sava|Lobby")
	FText DisplayName;

	//試合で使う GameMode(空のモードは選べるが開始できない。制作中のモード用)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sava|Lobby")
	TSoftClassPtr<AGameModeBase> GameModeClass;

	//開始に必要な人数(ホストを含む)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sava|Lobby", meta = (ClampMin = "1"))
	int32 MinPlayers = 2;

	//ラウンド制か(違えばルーム設定の「勝利ラウンド数」は使わない)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sava|Lobby")
	bool bUsesRounds = false;

	//キル数で勝敗が決まるか(違えばルーム設定の「勝利キル数」は使わない)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sava|Lobby")
	bool bUsesKillLimit = false;

	//個人戦・チーム戦を選べるか(違えばルーム設定の「チーム戦」は使わない)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sava|Lobby")
	bool bCanChooseTeamMatch = false;

	//このモードで遊べるマップ(CTF なら旗の台がある、など)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sava|Lobby")
	TArray<FSavaMatchMapOption> Maps;
};

//ロビー(待合室)とルーム設定の候補(Project Settings → Game → Sava Lobby)
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Sava Lobby"))
class SAVA_API USavaLobbySettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	//選べるモード。ルーム設定の ModeIndex はこの番号
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Modes")
	TArray<FSavaMatchModeOption> Modes;

	//勝利ラウンド数の範囲と最初の値
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Rules", meta = (ClampMin = "1"))
	int32 MinRoundsToWin = 1;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Rules", meta = (ClampMin = "1"))
	int32 MaxRoundsToWin = 5;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Rules", meta = (ClampMin = "1"))
	int32 DefaultRoundsToWin = 2;

	//勝利キル数の範囲と最初の値
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Rules", meta = (ClampMin = "1"))
	int32 MinKillLimit = 5;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Rules", meta = (ClampMin = "1"))
	int32 MaxKillLimit = 100;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Rules", meta = (ClampMin = "1"))
	int32 DefaultKillLimit = 30;

	//制限時間(秒)の範囲と最初の値
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Rules", meta = (ClampMin = "10", ForceUnits = "s"))
	int32 MinRoundTimeLimit = 60;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Rules", meta = (ClampMin = "10", ForceUnits = "s"))
	int32 MaxRoundTimeLimit = 600;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Rules", meta = (ClampMin = "10", ForceUnits = "s"))
	int32 DefaultRoundTimeLimit = 180;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	//部屋を作ったときのルーム設定
	FSavaRoomSettings MakeDefaultRoomSettings() const;

	//範囲外の値を直す(クライアントから届いた値は必ずこれを通す)
	FSavaRoomSettings Sanitize(const FSavaRoomSettings& InSettings) const;

	//番号のモード(なければ nullptr)
	const FSavaMatchModeOption* FindMode(int32 ModeIndex) const;

	//ルーム設定で試合を始められるか。だめなら理由を OutReason に入れる
	//Players は部屋にいる全員(ホストを含む)。ホスト以外の全員が準備 OK で、人数がモードの MinPlayers 以上なら開始できる
	bool CanStartMatch(const FSavaRoomSettings& RoomSettings, const TArray<APlayerState*>& Players, FText& OutReason) const;

	//---- UI(ルーム設定の画面)用

	//この設定(範囲の値などを読む用)
	UFUNCTION(BlueprintPure, Category = "Sava|Lobby", meta = (DisplayName = "Get Sava Lobby Settings"))
	static const USavaLobbySettings* GetLobbySettings() { return GetDefault<USavaLobbySettings>(); }

	UFUNCTION(BlueprintPure, Category = "Sava|Lobby")
	static TArray<FSavaMatchModeOption> GetModeOptions();

	//番号のモードを取得する(なければ false)
	UFUNCTION(BlueprintPure, Category = "Sava|Lobby")
	static bool GetModeOption(int32 ModeIndex, FSavaMatchModeOption& OutMode);

	//ルーム設定のモード名・マップ名(画面に出す用)
	UFUNCTION(BlueprintPure, Category = "Sava|Lobby")
	static void GetRoomSettingsDisplayNames(const FSavaRoomSettings& RoomSettings, FText& OutModeName, FText& OutMapName);
};

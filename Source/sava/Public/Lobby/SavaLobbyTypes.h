// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SavaLobbyTypes.generated.h"

//ホストがルーム設定の端末で決める、次の試合の設定(全員に同期する)
//モードとマップは Project Settings → Game → Sava Lobby の一覧の何番目か
USTRUCT(BlueprintType)
struct FSavaRoomSettings
{
	GENERATED_BODY()

	//モード(CTF・デスマッチなど)。USavaLobbySettings::Modes の番号
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sava|Lobby")
	int32 ModeIndex = 0;

	//試合マップ。選んだモードの Maps の番号
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sava|Lobby")
	int32 MapIndex = 0;

	//何ラウンド先取で勝ちか(ラウンド制のモードだけ使う)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sava|Lobby")
	int32 RoundsToWin = 2;

	//1 ラウンド(ラウンド制でなければ 1 試合)の制限時間(秒)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sava|Lobby", meta = (ForceUnits = "s"))
	int32 RoundTimeLimit = 180;

	bool operator==(const FSavaRoomSettings& Other) const
	{
		return ModeIndex == Other.ModeIndex && MapIndex == Other.MapIndex
			&& RoundsToWin == Other.RoundsToWin && RoundTimeLimit == Other.RoundTimeLimit;
	}
};

//試合を始めるときに試合マップへ渡すオプションの名前(試合の GameMode は InitGame の Options から読む)
namespace SavaMatchOptions
{
	//ラウンド先取数
	inline const TCHAR* RoundsToWin = TEXT("Rounds");
	//制限時間(秒)
	inline const TCHAR* TimeLimit = TEXT("TimeLimit");
	//ロビーから移ってくる人数(全員そろうまで試合を始めないのに使う)
	inline const TCHAR* ExpectedPlayers = TEXT("Players");
}

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "SavaOnlineSettings.generated.h"

//部屋(セッション)の設定(Project Settings → Game → Sava Online)
//部屋を抜けた・切断されたときは Project Settings → Maps & Modes の Game Default Map に戻る
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Sava Online"))
class SAVA_API USavaOnlineSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	//部屋を作ったときにホストが開くマップ(listen サーバーとして開く)
	UPROPERTY(Config, EditAnywhere, Category = "Maps")
	TSoftObjectPtr<UWorld> LobbyMap;

	//1 部屋の最大人数(ホストを含む)
	UPROPERTY(Config, EditAnywhere, Category = "Session", meta = (ClampMin = "2"))
	int32 MaxPlayers = 6;

	//部屋検索で受け取る最大件数
	UPROPERTY(Config, EditAnywhere, Category = "Session", meta = (ClampMin = "1"))
	int32 MaxSearchResults = 50;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }
};

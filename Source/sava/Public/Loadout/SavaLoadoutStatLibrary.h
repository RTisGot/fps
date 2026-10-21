// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Weapon/SavaWeaponTypes.h"
#include "SavaLoadoutStatLibrary.generated.h"

//比べた数値が良くなったか
UENUM(BlueprintType)
enum class ESavaStatChange : uint8
{
	//変わらない(表示した値が同じ)
	None,
	Better,
	Worse,
};

//ロードアウト画面の数値バー 1 行分(WBP_StatBar に渡す)
USTRUCT(BlueprintType)
struct SAVA_API FSavaStatRow
{
	GENERATED_BODY()

	//項目名(ダメージ・連射速度など)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sava|Loadout")
	FText Label;

	//値(単位付き。例: 25 / 600 / 2.0s)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sava|Loadout")
	FText ValueText;

	//バーの長さ(0 〜 1。長いほど良い。下がるほど良い数値(リロード時間など)は逆向きにしてある)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sava|Loadout")
	float Fill = 0.0f;

	//比べる数値のバーの長さ(0 〜 1。薄いバーを重ねて差を見せる用)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sava|Loadout")
	float CompareFill = 0.0f;

	//比べる数値からの増減(例: +2 / -30 / +0.2s。変わらなければ空)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sava|Loadout")
	FText DeltaText;

	//良くなったか(増減の色分け用)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sava|Loadout")
	ESavaStatChange Change = ESavaStatChange::None;
};

//ロードアウト画面で数値をバーにして見せるための計算
//バーの長さは、選べるもの全体(武器ならすべての武器の元の数値、キャラクターなら DT_Character のすべての行)の最大値を基準にする
UCLASS()
class SAVA_API USavaLoadoutStatLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//武器の数値の行(ダメージ・連射速度・マガジン・リロード・ADS 時間・射程・反動・腰撃ちの拡散)
	//Stats: 見せる数値 / CompareStats: 比べる数値(比べないなら Stats と同じものを渡す)
	UFUNCTION(BlueprintPure, Category = "Sava|Loadout")
	static TArray<FSavaStatRow> MakeWeaponStatRows(const FSavaWeaponStats& Stats, const FSavaWeaponStats& CompareStats);

	//キャラクターの数値の行(HP・移動速度・与ダメージ)。どちらも DT_Character の行名
	//CompareCharacter: 比べるキャラクター(比べないなら Character と同じものを渡す)
	UFUNCTION(BlueprintPure, Category = "Sava|Loadout")
	static TArray<FSavaStatRow> MakeCharacterStatRows(FName Character, FName CompareCharacter);
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "SavaCharacterTypes.generated.h"

class UTexture2D;

//ロードアウトで選べるキャラクター 1 体分の能力値(DT_Character の 1 行)
//行名(Light / Medium / Heavy など)がキャラクターの ID になり、ロードアウトにはこの行名を入れる
USTRUCT(BlueprintType)
struct SAVA_API FSavaCharacterRow : public FTableRowBase
{
	GENERATED_BODY()

	//ロードアウト画面に出す名前とアイコン
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sava|Character")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sava|Character")
	TObjectPtr<UTexture2D> Icon;

	//最大 HP
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sava|Character", meta = (ClampMin = "1"))
	float MaxHP = 100.0f;

	//歩きの最高速度(ダッシュ・スキルの倍率はこれに掛かる)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sava|Character", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float MoveSpeed = 800.0f;

	//当たり判定のカプセルの半径。0 なら体の既定値(BP_FirstPersonCharacter のカプセル)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sava|Character", meta = (ClampMin = "0", ForceUnits = "cm"))
	float CapsuleRadius = 55.0f;

	//立っているときのカプセルの高さの半分(中心から上端まで。上の丸い部分も含む)。半径より小さくはできない
	//0 なら体の既定値
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sava|Character", meta = (ClampMin = "0", ForceUnits = "cm"))
	float CapsuleHalfHeight = 96.0f;

	//しゃがみ中のカプセルの高さの半分。半径より小さくはできない(小さい値はエンジンが半径まで引き上げる)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sava|Character", meta = (ClampMin = "1", ForceUnits = "cm"))
	float CrouchedHalfHeight = 55.0f;

	//目(カメラ)をカプセルの上端からどれだけ下に置くか。立ち・しゃがみのどちらでも同じ距離になる
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sava|Character", meta = (ClampMin = "0", ForceUnits = "cm"))
	float EyeHeightBelowTop = 36.0f;

	//与えるダメージの倍率(武器・ガジェットなど、このプレイヤーが与えるすべてのダメージに掛かる)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sava|Character", meta = (ClampMin = "0"))
	float DamageMultiplier = 1.0f;
};

//ロードアウト画面の一覧の 1 項目(キャラクター用)
//キャラクターは DT の「行」でアセットではないため、武器などと同じ一覧部品(UObject を受け取る)で扱えるように包む
UCLASS(BlueprintType)
class SAVA_API USavaCharacterOption : public UObject
{
	GENERATED_BODY()

public:
	//DT_Character の行名
	UPROPERTY(BlueprintReadOnly, Category = "Sava|Character")
	FName CharacterId;
};

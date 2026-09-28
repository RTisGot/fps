// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SavaWeaponTypes.generated.h"

//武器の種類・性能
//※ 各数値の単位(RateOfFire が毎分か毎秒か、距離が cm か m か など)は未確認

//武器枠
UENUM(BlueprintType)
enum class ESavaWeaponSlot : uint8
{
	//メインウェポン
	Primary,
	//サブウェポン
	Secondary,
};

//撃ち方
UENUM(BlueprintType)
enum class ESavaFireMode : uint8
{
	SemiAuto,
	FullAuto,
	Burst,
};

//武器の種類
UENUM(BlueprintType)
enum class ESavaWeaponType : uint8
{
	Rifle,
	SMG,
	Shotgun,
	Sniper,
	Pistol,
	HeavyPistol,
};

//武器の性能
USTRUCT(BlueprintType)
struct SAVA_API FSavaWeaponStats
{
	GENERATED_BODY()

	//--------------------------------ダメージ

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float BaseDamage = 0.0f;

	//頭に当たったときのダメージ
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float HeadDamage = 0.0f;

	//--------------------------------弾数

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo")
	int32 MagazineSize = 0;

	//予備弾の最大数
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo")
	int32 MaxReserveAmmo = 0;

	//--------------------------------射撃

	//連射速度
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire")
	float RateOfFire = 0.0f;

	//--------------------------------拡散

	//腰撃ち時
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread")
	float HipSpread = 0.0f;

	//ADS 時
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread")
	float ADSSpread = 0.0f;

	//移動中
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread")
	float MovementSpread = 0.0f;

	//ジャンプ中
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread")
	float JumpSpread = 0.0f;

	//--------------------------------反動

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil")
	float VerticalRecoil = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil")
	float HorizontalRecoil = 0.0f;

	//反動から戻る速さ
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil")
	float RecoilRecovery = 0.0f;

	//--------------------------------ADS

	//構えるまでの時間
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ADS")
	float ADSTime = 0.0f;

	//構えたときのズーム
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ADS")
	float ADSZoom = 0.0f;

	//--------------------------------距離減衰

	//ダメージが減り始める距離
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Falloff")
	float FalloffStartDistance = 0.0f;

	//ダメージが最低になる距離
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Falloff")
	float FalloffEndDistance = 0.0f;

	//距離減衰後の最低ダメージ
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Falloff")
	float MinimumDamage = 0.0f;

	//--------------------------------ヒットスキャン

	//1 回の射撃で出る弾の数
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitScan")
	int32 PelletCount = 0;

	//当たり判定の太さ
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitScan")
	float HitScanRadius = 0.0f;
};

//性能の補正(アタッチメントなど)。Multiplier は掛ける値、Add は足す値
//※ BP 版では Multiplier の初期値が 0 だが、0 を掛けると性能が消えるため C++ 版は 1(補正なし)にしている
USTRUCT(BlueprintType)
struct SAVA_API FSavaWeaponModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float DamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float HeadDamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo")
	int32 MagazineSizeAdd = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo")
	int32 MaxReserveAmmoAdd = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire")
	float RateOfFireMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread")
	float HipSpreadMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread")
	float ADSSpreadMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread")
	float MovementSpreadMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread")
	float JumpSpreadMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil")
	float VerticalRecoilMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil")
	float HorizontalRecoilMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil")
	float RecoilRecoveryMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ADS")
	float ADSTimeMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ADS")
	float ADSZoomMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Falloff")
	float FalloffStartMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Falloff")
	float FalloffEndMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Falloff")
	float MinimumDamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitScan")
	float HitScanRadiusMultiplier = 1.0f;
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SavaWeaponTypes.generated.h"

//撃ち方(BP の E_FireMode を移したもの)
UENUM(BlueprintType)
enum class ESavaFireMode : uint8
{
	//押すたびに 1 発
	SemiAuto,
	//押すたびに Burst Count 発
	Burst,
	//押している間撃ち続ける
	FullAuto,
};

//武器の種類(BP の E_WeaponType を移したもの)
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

//武器の枠(BP の E_WeaponSlot を移したもの)
UENUM(BlueprintType)
enum class ESavaWeaponSlot : uint8
{
	//メイン武器
	Primary,
	//サブ武器(予備弾は無限)
	Secondary,
};

//武器の数値(BP の ST_WeaponStats を移したもの。Reload Time と Burst Count は追加)
USTRUCT(BlueprintType)
struct SAVA_API FSavaWeaponStats
{
	GENERATED_BODY()

	//--------------------------------Damage

	//胴体への 1 発のダメージ(減衰前)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage", meta = (ClampMin = "0"))
	float BaseDamage = 25.0f;

	//頭への 1 発のダメージ(減衰前)。距離による減衰は胴体と同じ割合で下がる
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage", meta = (ClampMin = "0"))
	float HeadDamage = 40.0f;

	//--------------------------------Ammo

	//マガジンの弾数
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo", meta = (ClampMin = "1"))
	int32 MagazineSize = 30;

	//予備弾の最大数(サブ武器は無限なので使わない)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo", meta = (ClampMin = "0"))
	int32 MaxReserveAmmo = 120;

	//連射速度(毎分の発射数。600 なら 0.1 秒間隔)。バーストの中の間隔もこれを使う
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo", meta = (ClampMin = "1"))
	float RateOfFire = 600.0f;

	//リロードにかかる時間
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo", meta = (ClampMin = "0", ForceUnits = "s"))
	float ReloadTime = 2.0f;

	//バースト 1 回の発射数(Fire Mode が Burst のときだけ使う)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo", meta = (ClampMin = "1"))
	int32 BurstCount = 3;

	//--------------------------------Spread(撃つ方向が最大この角度だけランダムにずれる)

	//腰撃ちの拡散
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread", meta = (ClampMin = "0", ForceUnits = "deg"))
	float HipSpread = 2.0f;

	//ADS 中の拡散(腰撃ちの値の代わりに使う)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread", meta = (ClampMin = "0", ForceUnits = "deg"))
	float ADSSpread = 0.3f;

	//移動中に足す拡散(歩く速さで最大になる)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread", meta = (ClampMin = "0", ForceUnits = "deg"))
	float MovementSpread = 1.5f;

	//空中にいるときに足す拡散
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread", meta = (ClampMin = "0", ForceUnits = "deg"))
	float JumpSpread = 4.0f;

	//ADS 中は、移動中・空中の拡散をこの割合に減らす(0.5 なら半分、1 なら減らさない。ADS に入るほど近づく)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread", meta = (ClampMin = "0", ClampMax = "1"))
	float ADSMovementSpreadScale = 0.5f;

	//--------------------------------Recoil(1 発ごとに視点がランダムに動く)

	//上への跳ね上がり(0 〜 この値)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil", meta = (ClampMin = "0", ForceUnits = "deg"))
	float VerticalRecoil = 0.6f;

	//左右のぶれ(-この値 〜 +この値)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil", meta = (ClampMin = "0", ForceUnits = "deg"))
	float HorizontalRecoil = 0.3f;

	//撃ち終わった後に、跳ね上がった分が戻る速さ(度/秒)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil", meta = (ClampMin = "0"))
	float RecoilRecovery = 10.0f;

	//--------------------------------ADS

	//ADS に入りきるまでの時間
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ADS", meta = (ClampMin = "0", ForceUnits = "s"))
	float ADSTime = 0.2f;

	//ADS 中の視野角の倍率(0.8 なら 80%)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ADS", meta = (ClampMin = "0.05", ClampMax = "1"))
	float ADSZoom = 0.8f;

	//--------------------------------Falloff(Start まではそのまま、End までに Minimum Damage まで直線的に下がる)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Falloff", meta = (ClampMin = "0", ForceUnits = "cm"))
	float FalloffStartDistance = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Falloff", meta = (ClampMin = "0", ForceUnits = "cm"))
	float FalloffEndDistance = 4000.0f;

	//End より遠いときの胴体へのダメージ
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Falloff", meta = (ClampMin = "0"))
	float MinimumDamage = 15.0f;

	//--------------------------------Pellet

	//1 回に撃つ弾の数(ショットガン用)。1 発ごとに拡散し、ダメージも 1 発ずつ計算する
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pellet", meta = (ClampMin = "1"))
	int32 PelletCount = 1;

	//当たり判定の太さ(半径)。0 なら細いレイ
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pellet", meta = (ClampMin = "0", ForceUnits = "cm"))
	float HitScanRadius = 0.0f;

	//1 発ごとの間隔(秒)
	float GetFireInterval() const;

	//距離と部位からダメージを計算する
	float CalculateDamage(float Distance, bool bHeadshot) const;

	//拡散の角度を計算する
	//ADSAlpha: ADS の進み具合(0 〜 1) / MoveAlpha: 歩く速さに対する今の速さ(0 〜 1) / bInAir: 空中にいるか
	float CalculateSpread(float ADSAlpha, float MoveAlpha, bool bInAir) const;
};

//武器のカスタム(アタッチメントなど)で数値を変える量(BP の ST_WeaponModifier を移したもの。Reload Time Multiplier は追加)
//Multiplier は掛け算(1 で変化なし)、Add は足し算(0 で変化なし)。複数付けた場合は、掛け算は掛け合わせ、足し算は合計する
USTRUCT(BlueprintType)
struct SAVA_API FSavaWeaponModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage", meta = (ClampMin = "0"))
	float DamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage", meta = (ClampMin = "0"))
	float HeadDamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo")
	int32 MagazineSizeAdd = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo")
	int32 MaxReserveAmmoAdd = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo", meta = (ClampMin = "0"))
	float RateOfFireMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo", meta = (ClampMin = "0"))
	float ReloadTimeMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread", meta = (ClampMin = "0"))
	float HipSpreadMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread", meta = (ClampMin = "0"))
	float ADSSpreadMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread", meta = (ClampMin = "0"))
	float MovementSpreadMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread", meta = (ClampMin = "0"))
	float JumpSpreadMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread", meta = (ClampMin = "0"))
	float ADSMovementSpreadScaleMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil", meta = (ClampMin = "0"))
	float VerticalRecoilMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil", meta = (ClampMin = "0"))
	float HorizontalRecoilMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil", meta = (ClampMin = "0"))
	float RecoilRecoveryMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ADS", meta = (ClampMin = "0"))
	float ADSTimeMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ADS", meta = (ClampMin = "0"))
	float ADSZoomMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Falloff", meta = (ClampMin = "0"))
	float FalloffStartMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Falloff", meta = (ClampMin = "0"))
	float FalloffEndMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Falloff", meta = (ClampMin = "0"))
	float MinimumDamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pellet", meta = (ClampMin = "0"))
	float HitScanRadiusMultiplier = 1.0f;
};

//武器の数値の計算(Blueprint からも呼べる)
UCLASS()
class SAVA_API USavaWeaponLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//元の数値にカスタムを適用した数値を返す
	UFUNCTION(BlueprintPure, Category = "Sava|Weapon")
	static FSavaWeaponStats ApplyWeaponModifiers(const FSavaWeaponStats& BaseStats, const TArray<FSavaWeaponModifier>& Modifiers);

	//1 発ごとの間隔(秒)
	UFUNCTION(BlueprintPure, Category = "Sava|Weapon")
	static float GetFireInterval(const FSavaWeaponStats& Stats) { return Stats.GetFireInterval(); }

	//距離と部位からダメージを計算する
	UFUNCTION(BlueprintPure, Category = "Sava|Weapon")
	static float CalculateWeaponDamage(const FSavaWeaponStats& Stats, float Distance, bool bHeadshot) { return Stats.CalculateDamage(Distance, bHeadshot); }
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SavaWeaponTypes.generated.h"

class UTexture2D;

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

//拡散を決めるための、キャラクターの動き方
UENUM(BlueprintType)
enum class ESavaSpreadMovement : uint8
{
	//歩き・止まっている・空中
	Walk,
	//ダッシュ・加速スキルなどで速くなっている
	Sprint,
	Slide,
	//グラップルで引っ張られている
	Grapple,
};

//リロードのやり方
UENUM(BlueprintType)
enum class ESavaReloadType : uint8
{
	//マガジンごと替える(途中の「弾が入る瞬間」に全部入る)
	Magazine,
	//1 発ずつ込める(ショットガンなど。途中で撃つと、込めた分で撃てる)
	PerRound,
};

//今の拡散の内訳(すべて ADS の減らし方を掛けた後の角度。Total = Base + Movement + Shot)
USTRUCT(BlueprintType)
struct SAVA_API FSavaSpreadBreakdown
{
	GENERATED_BODY()

	//腰撃ち・ADS の基本の拡散(1 発目なら First Shot Spread Scale を掛けた後)
	UPROPERTY(BlueprintReadOnly, Category = "Spread")
	float Base = 0.0f;

	//移動・空中の拡散
	UPROPERTY(BlueprintReadOnly, Category = "Spread")
	float Movement = 0.0f;

	//連射の拡散
	UPROPERTY(BlueprintReadOnly, Category = "Spread")
	float Shot = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Spread")
	float Total = 0.0f;

	//1 発目の精度が効いているか
	UPROPERTY(BlueprintReadOnly, Category = "Spread")
	bool bFirstShot = false;
};

//武器の数値(BP の ST_WeaponStats を移したもの。Reload Time と Burst Count は追加)
//Category の「A|B」は、データアセットでは A のグループの中の B のグループとして表示される(savaEditor の FSavaWeaponDataCustomization)
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

	//持ち替えでこの武器を出してから、撃つ・ADS・リロードができるまでの時間(前の武器をしまう時間もここに含める)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo", meta = (ClampMin = "0", ForceUnits = "s"))
	float EquipTime = 0.4f;

	//ダッシュをやめてから撃てるようになるまでの時間(ダッシュ中に撃とうとすると、ダッシュが止まってからこの時間後に撃つ)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo", meta = (ClampMin = "0", ForceUnits = "s"))
	float SprintToFireTime = 0.2f;

	//バースト 1 回の発射数(Fire Mode が Burst のときだけ使う)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo", meta = (ClampMin = "1"))
	int32 BurstCount = 3;

	//--------------------------------Reload(時間はすべて、カスタムの Reload Time Multiplier が掛かる)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reload")
	ESavaReloadType ReloadType = ESavaReloadType::Magazine;

	//弾が残っているときのリロードにかかる時間
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reload", meta = (ClampMin = "0", ForceUnits = "s", EditCondition = "ReloadType == ESavaReloadType::Magazine", EditConditionHides))
	float ReloadTime = 2.0f;

	//マガジンが空のときのリロードにかかる時間(コッキングの分だけ長くする)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reload", meta = (ClampMin = "0", ForceUnits = "s", EditCondition = "ReloadType == ESavaReloadType::Magazine", EditConditionHides))
	float EmptyReloadTime = 2.5f;

	//リロード時間のうち、弾が入る瞬間(0.75 なら 75% の時点)。空のときも同じ割合
	//それより前に持ち替えると弾は入らない。弾が入った後は、撃つ・ダッシュで残りの動作を飛ばせる
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reload", meta = (ClampMin = "0", ClampMax = "1", EditCondition = "ReloadType == ESavaReloadType::Magazine", EditConditionHides))
	float ReloadInsertRatio = 0.75f;

	//1 発ずつ込める: 込め始めるまでの時間(構える動作)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reload", meta = (ClampMin = "0", ForceUnits = "s", EditCondition = "ReloadType == ESavaReloadType::PerRound", EditConditionHides))
	float ReloadStartTime = 0.4f;

	//1 発ずつ込める: 1 発込めるのにかかる時間(この時間が経つたびに 1 発入る)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reload", meta = (ClampMin = "0.01", ForceUnits = "s", EditCondition = "ReloadType == ESavaReloadType::PerRound", EditConditionHides))
	float ReloadRoundTime = 0.5f;

	//1 発ずつ込める: 込め終わってから撃てるようになるまでの時間(撃つ・ダッシュで飛ばせる)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reload", meta = (ClampMin = "0", ForceUnits = "s", EditCondition = "ReloadType == ESavaReloadType::PerRound", EditConditionHides))
	float ReloadEndTime = 0.4f;

	//薬室の 1 発: 弾が残っている状態でリロードすると、マガジンの弾数 + 1 発まで入る
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reload")
	bool bChamberRound = false;

	//--------------------------------Spread(撃つ方向が最大この角度だけランダムにずれる。角度 = 基本 + 移動 + 連射)

	//腰撃ちの拡散
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread|Base", meta = (ClampMin = "0", ForceUnits = "deg"))
	float HipSpread = 2.0f;

	//ADS 中の拡散(腰撃ちの値の代わりに使う)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread|Base", meta = (ClampMin = "0", ForceUnits = "deg"))
	float ADSSpread = 0.3f;

	//拡散の円の、半径の半分より内側に飛ぶ弾の割合(0.25 = 円の中に均等に散る / 0.5 = 半分が内側 / 大きいほど中心に集まる)
	//中心に集めると、円の大きさ(クロスヘア)は同じでも当たりやすくなる
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread|Base", meta = (ClampMin = "0.05", ClampMax = "0.95"))
	float SpreadCenterRatio = 0.5f;

	//1 発目(撃つのをやめて Recoil Reset Time 経った後の 1 発)の、基本の拡散の倍率(0 なら 1 発目はまっすぐ飛ぶ、1 なら変わらない)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread|Base", meta = (ClampMin = "0", ClampMax = "1"))
	float FirstShotSpreadScale = 1.0f;

	//歩いているときに足す拡散(歩く速さで最大になる)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread|Movement", meta = (ClampMin = "0", ForceUnits = "deg"))
	float MovementSpread = 1.5f;

	//ダッシュ中に足す拡散(歩きの拡散の代わりに使う)。加速スキルなどで速くなっている間は、歩いていてもこちらを使う(速さに合わせて最大になる)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread|Movement", meta = (ClampMin = "0", ForceUnits = "deg"))
	float SprintSpread = 3.0f;

	//スライディング中に足す拡散(歩きの拡散の代わりに使う)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread|Movement", meta = (ClampMin = "0", ForceUnits = "deg"))
	float SlideSpread = 2.0f;

	//グラップルで引っ張られている間に足す拡散(歩き・空中の拡散の代わりに使う)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread|Movement", meta = (ClampMin = "0", ForceUnits = "deg"))
	float GrappleSpread = 4.0f;

	//空中にいるときに足す拡散
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread|Movement", meta = (ClampMin = "0", ForceUnits = "deg"))
	float JumpSpread = 4.0f;

	//ADS 中は、移動中・空中の拡散をこの割合に減らす(0.5 なら半分、1 なら減らさない。ADS に入るほど近づく)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread|Movement", meta = (ClampMin = "0", ClampMax = "1"))
	float ADSMovementSpreadScale = 0.5f;

	//移動・空中の拡散が広がるときに、だいたい追いつくまでの時間(0 なら一瞬で広がる)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread|Movement", meta = (ClampMin = "0", ForceUnits = "s"))
	float MovementSpreadInTime = 0.1f;

	//移動・空中の拡散が戻る(止まる・着地する)ときに、だいたい戻りきるまでの時間(0 なら一瞬で戻る)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread|Movement", meta = (ClampMin = "0", ForceUnits = "s"))
	float MovementSpreadOutTime = 0.25f;

	//連射の拡散: 1 発撃つごとに足す拡散(1 発目は足す前の拡散で撃つ)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread|Shot", meta = (ClampMin = "0", ForceUnits = "deg"))
	float SpreadPerShot = 0.25f;

	//連射の拡散の上限
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread|Shot", meta = (ClampMin = "0", ForceUnits = "deg"))
	float MaxShotSpread = 2.0f;

	//連射の拡散が戻る速さ(度/秒)。撃ち続けている間は戻らない
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread|Shot", meta = (ClampMin = "0"))
	float ShotSpreadRecovery = 8.0f;

	//撃ってから連射の拡散が戻り始めるまでの時間(連射間隔より短い値は連射間隔として扱う)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread|Shot", meta = (ClampMin = "0", ForceUnits = "s"))
	float ShotSpreadRecoveryDelay = 0.0f;

	//ADS 中は、連射の拡散をこの割合に減らす(0.5 なら半分、1 なら減らさない。ADS に入るほど近づく)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread|Shot", meta = (ClampMin = "0", ClampMax = "1"))
	float ADSShotSpreadScale = 0.5f;

	//--------------------------------Recoil(1 発ごとに、Recoil Pattern に沿って視点が動く)

	//パターンの縦の値 1 につき、上へ跳ね上がる角度
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil", meta = (ClampMin = "0", ForceUnits = "deg"))
	float VerticalRecoil = 0.6f;

	//パターンの横の値 1 につき、横へ動く角度
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil", meta = (ClampMin = "0", ForceUnits = "deg"))
	float HorizontalRecoil = 0.3f;

	//撃ち始めから n 発目に視点が動く量(X = 横(+ で右)、Y = 縦(+ で上))。実際の角度は X × Horizontal Recoil、Y × Vertical Recoil
	//例: (0, 1) (0, 1) (0.5, 1) (-0.5, 1) … なら、2 発まっすぐ上がってから左右に揺れる。空なら毎発 (0, 1)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil")
	TArray<FVector2D> RecoilPattern;

	//パターンの最後まで撃ったら、この番号(0 = 最初)の 1 発から繰り返す
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil", meta = (ClampMin = "0"))
	int32 RecoilPatternLoopStart = 0;

	//パターンの値に足すランダムなずれ(-この値 〜 +この値。0 ならいつも同じ形、0.1 なら 1 発分の 1 割くらいずれる)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil", meta = (ClampMin = "0", ClampMax = "1"))
	float RecoilRandomness = 0.1f;

	//撃つのをやめてからこの時間が経つと、パターンが最初の 1 発目に戻る(連射間隔より短いと、毎発 1 発目になる)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil", meta = (ClampMin = "0", ForceUnits = "s"))
	float RecoilResetTime = 0.3f;

	//1 発分の反動を視点に足しきるまでの時間(最初は速く、だんだんゆっくり動く。0 なら一瞬で動く)
	//連射間隔より長いと、次の弾は上がりきる前の向きに飛ぶ
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil", meta = (ClampMin = "0", ForceUnits = "s"))
	float RecoilKickTime = 0.06f;

	//見た目だけの揺れ(画面と腕が一瞬跳ねて、バネのように戻る。弾の向きは変わらない)の大きさ。1 発の反動の角度にこの値を掛ける(0 なら揺れない)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil|Visual", meta = (ClampMin = "0"))
	float VisualKickScale = 0.5f;

	//見た目だけの揺れで、画面が傾く角度(-この値 〜 +この値でランダム)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil|Visual", meta = (ClampMin = "0", ForceUnits = "deg"))
	float VisualKickRoll = 0.4f;

	//見た目だけの揺れが一番大きくなるまでの時間(戻りきるのはこの 5 倍くらい)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil|Visual", meta = (ClampMin = "0.005", ForceUnits = "s"))
	float VisualKickTime = 0.04f;

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

	//ADS 中の移動速度の倍率(0.7 なら 70%。ADS ボタンを押した瞬間から効く。スライディング中は変わらない)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ADS", meta = (ClampMin = "0.1", ClampMax = "1"))
	float ADSMoveSpeed = 0.7f;

	//--------------------------------Falloff(Start まではそのまま、End までに Minimum Damage まで直線的に下がる)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Falloff", meta = (ClampMin = "0", ForceUnits = "cm"))
	float FalloffStartDistance = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Falloff", meta = (ClampMin = "0", ForceUnits = "cm"))
	float FalloffEndDistance = 4000.0f;

	//End より遠いときの胴体へのダメージ
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Falloff", meta = (ClampMin = "0"))
	float MinimumDamage = 15.0f;

	//--------------------------------Pellet

	//1 回に撃つ弾の数(ショットガン用)。ダメージは 1 発ずつ計算する
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pellet", meta = (ClampMin = "1"))
	int32 PelletCount = 1;

	//弾が 2 発以上のとき、1 回分の弾が広がる角度(拡散とは別。拡散で狙いがずれた位置を中心に、この円の中に均等に散る)
	//拡散は「狙いのブレ」、こちらは「粒の広がり」で、ADS や移動では変わらない
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pellet", meta = (ClampMin = "0", ForceUnits = "deg", EditCondition = "PelletCount > 1", EditConditionHides))
	float PelletSpread = 3.0f;

	//当たり判定の太さ(半径)。0 なら細いレイ
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pellet", meta = (ClampMin = "0", ForceUnits = "cm"))
	float HitScanRadius = 0.0f;

	//1 発ごとの間隔(秒)
	float GetFireInterval() const;

	//距離と部位からダメージを計算する
	float CalculateDamage(float Distance, bool bHeadshot) const;

	//移動・空中の拡散(ADS で減らす前)。自分の画面では、これを Movement Spread In/Out Time で滑らかにしてから使う
	//MoveAlpha: 歩く速さに対する今の速さ(0 〜 1) / bInAir: 空中にいるか / Movement: どう動いているか(歩き以外は、その動きの拡散を歩きの拡散の代わりに使う)
	float CalculateMovementSpread(float MoveAlpha, bool bInAir, ESavaSpreadMovement Movement = ESavaSpreadMovement::Walk) const;

	//移動・空中の拡散を、Target へ DeltaTime 秒分だけ近づける(広がるときは In Time、戻るときは Out Time でだいたい追いつく)
	float BlendMovementSpread(float Current, float Target, float DeltaTime) const;

	//拡散の角度とその内訳を計算する
	//ADSAlpha: ADS の進み具合(0 〜 1) / InMovementSpread: 移動・空中の拡散(CalculateMovementSpread) / ShotSpread: 今の連射の拡散 / bFirstShot: 1 発目か
	FSavaSpreadBreakdown CalculateSpreadBreakdown(float ADSAlpha, float InMovementSpread, float ShotSpread, bool bFirstShot = false) const;

	//最後に撃ってから TimeSinceLastShot 秒後の、連射の拡散(撃った直後が ShotSpreadAtLastShot)
	float CalculateShotSpread(float ShotSpreadAtLastShot, float TimeSinceLastShot) const;

	//拡散 SpreadAngle の中で、狙いからどれだけずらすか(度。X = 横(+ で右)、Y = 縦(+ で上))。Spread Center Ratio に合わせて中心に集める
	//RandRadius・RandAngle: 0 〜 1 の乱数
	FVector2D SampleSpreadOffset(float SpreadAngle, float RandRadius, float RandAngle) const;

	//円 ConeAngle の中の位置(度)。CenterRatio = 半径の半分より内側に入る割合(0.25 で均等)
	static FVector2D SampleConeOffset(float ConeAngle, float CenterRatio, float RandRadius, float RandAngle);

	//--------------------------------リロード

	//マガジン式のリロードにかかる時間(bEmpty = マガジンが空)
	float GetReloadDuration(bool bEmpty) const;

	//マガジン式のリロードで、弾が入るまでの時間
	float GetReloadInsertTime(bool bEmpty) const;

	//リロードで入る弾数の上限(bRoundChambered = リロードを始めたときに弾が残っていた。薬室の 1 発があれば +1)
	int32 GetMagazineCapacity(bool bRoundChambered) const;

	//撃ち始めから ShotIndex 発目(0 = 1 発目)の反動の角度を計算する(X = 横(+ で右)、Y = 縦(+ で上))
	//Noise: ランダムなずれ(-1 〜 +1。Recoil Randomness を掛けてからパターンの値に足す)
	FVector2D CalculateRecoilKick(int32 ShotIndex, const FVector2D& Noise) const;
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

	//リロードの時間すべて(残弾あり・空・1 発ずつの各時間)に掛かる
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reload", meta = (ClampMin = "0"))
	float ReloadTimeMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo", meta = (ClampMin = "0"))
	float EquipTimeMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo", meta = (ClampMin = "0"))
	float SprintToFireTimeMultiplier = 1.0f;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread", meta = (ClampMin = "0"))
	float ADSShotSpreadScaleMultiplier = 1.0f;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ADS", meta = (ClampMin = "0"))
	float ADSMoveSpeedMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Falloff", meta = (ClampMin = "0"))
	float FalloffStartMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Falloff", meta = (ClampMin = "0"))
	float FalloffEndMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Falloff", meta = (ClampMin = "0"))
	float MinimumDamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pellet", meta = (ClampMin = "0"))
	float PelletSpreadMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pellet", meta = (ClampMin = "0"))
	float HitScanRadiusMultiplier = 1.0f;
};

//--------------------------------カスタマイズ(武器データに並べて、武器ごとに中身を変えられる)

//スライダー 1 本の定義
//スライダーの値は -1 〜 +1。+1 で Max Modifier、-1 で Min Modifier がそのまま効き、途中の値はその間になる(0 で変化なし)
//Max と Min に逆向きの効果を入れると「何かを強くする代わりに何かが弱くなる」スライダーになる
USTRUCT(BlueprintType)
struct SAVA_API FSavaWeaponTuningSlider
{
	GENERATED_BODY()

	//保存・通信に使う名前(同じ武器の中で重ならないようにする。変えると、プレイヤーが決めた値は 0 に戻る)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tuning")
	FName Id;

	//画面に出す名前(例: バレル)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tuning")
	FText DisplayName;

	//左端(-1)・右端(+1)に出す説明(例: 取り回し / 射程)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tuning")
	FText MinLabel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tuning")
	FText MaxLabel;

	//左端(-1)まで振ったときの効果
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tuning")
	FSavaWeaponModifier MinModifier;

	//右端(+1)まで振ったときの効果
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tuning")
	FSavaWeaponModifier MaxModifier;
};

//切り替え式のカスタマイズの選択肢 1 つ(例: ホロサイト)
USTRUCT(BlueprintType)
struct SAVA_API FSavaWeaponTuningOption
{
	GENERATED_BODY()

	//保存・通信に使う名前(同じ切り替えの中で重ならないようにする)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tuning")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tuning")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tuning")
	TObjectPtr<UTexture2D> Icon;

	//これを選んだときの効果(サイトなら ADS Zoom Multiplier・ADS Time Multiplier など)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tuning")
	FSavaWeaponModifier Modifier;
};

//切り替え式のカスタマイズ 1 つ(例: サイト)。選択肢からどれか 1 つを選ぶ。最初の選択肢が初期値
USTRUCT(BlueprintType)
struct SAVA_API FSavaWeaponTuningToggle
{
	GENERATED_BODY()

	//保存・通信に使う名前(同じ武器の中で重ならないようにする)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tuning")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tuning")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tuning", meta = (TitleProperty = "Id"))
	TArray<FSavaWeaponTuningOption> Options;
};

//プレイヤーが決めたスライダー 1 本の値
USTRUCT(BlueprintType)
struct SAVA_API FSavaWeaponTuningSliderValue
{
	GENERATED_BODY()

	//FSavaWeaponTuningSlider の Id
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	FName Slider;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning", meta = (ClampMin = "-1", ClampMax = "1"))
	float Value = 0.0f;

	bool operator==(const FSavaWeaponTuningSliderValue& Other) const { return Slider == Other.Slider && Value == Other.Value; }
};

//プレイヤーが切り替えで選んだもの
USTRUCT(BlueprintType)
struct SAVA_API FSavaWeaponTuningToggleValue
{
	GENERATED_BODY()

	//FSavaWeaponTuningToggle の Id
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	FName Toggle;

	//FSavaWeaponTuningOption の Id
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	FName Option;

	bool operator==(const FSavaWeaponTuningToggleValue& Other) const { return Toggle == Other.Toggle && Option == Other.Option; }
};

//プレイヤーが決めた 1 つの武器のカスタマイズ(Id で持つので、武器データで並べ替えても保存した値は残る)
//値の範囲や、その武器にある Id かどうかは USavaWeaponData::SanitizeTuning で直す
USTRUCT(BlueprintType)
struct SAVA_API FSavaWeaponTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	TArray<FSavaWeaponTuningSliderValue> Sliders;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	TArray<FSavaWeaponTuningToggleValue> Toggles;

	//無ければ 0
	float GetSliderValue(FName Slider) const;
	void SetSliderValue(FName Slider, float Value);

	//無ければ None
	FName GetToggleOption(FName Toggle) const;
	void SetToggleOption(FName Toggle, FName Option);

	bool operator==(const FSavaWeaponTuning& Other) const { return Sliders == Other.Sliders && Toggles == Other.Toggles; }
	bool operator!=(const FSavaWeaponTuning& Other) const { return !(*this == Other); }
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

	//スライダーの値(-1 〜 +1)を、1 つのカスタムにする
	//掛け算は「端の倍率 ^ 振った量」、足し算は「端の値 × 振った量」を丸めたもの
	UFUNCTION(BlueprintPure, Category = "Sava|Weapon")
	static FSavaWeaponModifier MakeSliderModifier(const FSavaWeaponTuningSlider& Slider, float Value);

	//-1 〜 +1 に収め、0.01 刻みに丸めた値(クライアントから届いた値・保存した値に使う)
	UFUNCTION(BlueprintPure, Category = "Sava|Weapon")
	static float SanitizeSliderValue(float Value);

	//カスタマイズの値を読み書きする(スライダー・切り替えの Widget から使う)
	UFUNCTION(BlueprintPure, Category = "Sava|Weapon")
	static float GetTuningSliderValue(const FSavaWeaponTuning& Tuning, FName Slider) { return Tuning.GetSliderValue(Slider); }

	UFUNCTION(BlueprintPure, Category = "Sava|Weapon")
	static FSavaWeaponTuning SetTuningSliderValue(FSavaWeaponTuning Tuning, FName Slider, float Value);

	UFUNCTION(BlueprintPure, Category = "Sava|Weapon")
	static FName GetTuningToggleOption(const FSavaWeaponTuning& Tuning, FName Toggle) { return Tuning.GetToggleOption(Toggle); }

	UFUNCTION(BlueprintPure, Category = "Sava|Weapon")
	static FSavaWeaponTuning SetTuningToggleOption(FSavaWeaponTuning Tuning, FName Toggle, FName Option);

	//1 発ごとの間隔(秒)
	UFUNCTION(BlueprintPure, Category = "Sava|Weapon")
	static float GetFireInterval(const FSavaWeaponStats& Stats) { return Stats.GetFireInterval(); }

	//距離と部位からダメージを計算する
	UFUNCTION(BlueprintPure, Category = "Sava|Weapon")
	static float CalculateWeaponDamage(const FSavaWeaponStats& Stats, float Distance, bool bHeadshot) { return Stats.CalculateDamage(Distance, bHeadshot); }
};

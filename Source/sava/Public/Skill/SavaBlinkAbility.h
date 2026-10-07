#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/SavaHoldAimAbility.h"
#include "SavaBlinkAbility.generated.h"

class ACharacter;

/**
 * スキル「ブリンク」。押している間は移動先を表示し、離すとそこへ瞬間移動する。
 *
 * ・届く距離は Max Range(既定 1500 cm = 15 m)。視線の先の壁・床の手前に止まる(壁は通り抜けない)
 * ・移動前の速度(慣性)は移動後もそのまま残る
 * ・狙っている間に右クリック(Cancel Input Tag)でキャンセル。キャンセル・移動できない場所ではクールダウンを消費しない
 *
 * 仕組み:
 *     本当にワープ(SetActorLocation)すると、通信対戦でクライアントとサーバーの位置がずれて引き戻されるため、
 *     Blink Duration(既定 0.1 秒)の間に超高速で移動させる(グラップルと同じ Root Motion Source を使う)。
 *     自分の画面ではすぐ動き(予測)、サーバーは届いた移動先を確認してから同じ移動をする。
 *
 * 使い方:
 *     USavaSkillData のデータアセットを作り、Ability にこのクラス(か Blueprint の子クラス)、
 *     Input Tag にスキルのボタン(例: InputTag.Skill)を入れる。
 */
UCLASS()
class SAVA_API USavaBlinkAbility : public USavaHoldAimAbility
{
	GENERATED_BODY()

public:
	USavaBlinkAbility();

	/**
	 * 移動先(カプセルの中心)を探す。テストからも使う
	 * ViewLocation / ViewDirection: 視点の位置と向き
	 * 戻り値: 移動できるか(false でも OutTarget には表示用の位置が入る)
	 */
	static bool FindBlinkTarget(const ACharacter* Character, const FVector& ViewLocation, const FVector& ViewDirection,
		float Range, float SurfaceClearance, float MinDistance, FVector& OutTarget);

protected:
	virtual bool ComputeAim(FTransform& OutTransform, TArray<FVector>& OutPathPoints) const override;
	virtual bool IsTargetAllowed_Implementation(const FTransform& TargetTransform) const override;
	virtual void OnAimUpdated_Implementation(const FTransform& AimTransform, bool bIsValid, const TArray<FVector>& PathPoints) override;
	virtual void OnConfirmed_Implementation(const FTransform& TargetTransform) override;
	virtual void OnConfirmedPredicted_Implementation(const FTransform& TargetTransform) override;

	// 移動にかける時間。短いほど瞬間移動に近い(短すぎると細い障害物に引っかかりやすい)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Blink", meta = (ClampMin = "0.02", ClampMax = "0.5", ForceUnits = "s"))
	float BlinkDuration = 0.1f;

	// 壁・床からどれだけ離して止まるか
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Blink", meta = (ClampMin = "1", ForceUnits = "cm"))
	float SurfaceClearance = 10.0f;

	// これより近い移動先は無効(目の前が壁のときなど)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Blink", meta = (ClampMin = "0", ForceUnits = "cm"))
	float MinBlinkDistance = 100.0f;

	/**
	 * 移動先を簡易表示(デバッグ線のカプセル)するか。
	 * 見た目をちゃんと作るときは false にして、Blueprint で On Aim Started / Updated / Ended を実装する。
	 * デバッグ線は配布用(Shipping)ビルドでは表示されない。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Blink|Preview")
	bool bDrawDebugPreview = true;

	// 移動を始めた(自分の画面とサーバー)。音・エフェクト用
	UFUNCTION(BlueprintImplementableEvent, Category = "Sava|Blink")
	void OnBlinkStarted(FVector From, FVector To);

private:
	void StartBlink(const FTransform& TargetTransform);
};

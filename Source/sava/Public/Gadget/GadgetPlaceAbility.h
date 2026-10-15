#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/SavaHoldAimAbility.h"
#include "GadgetPlaceAbility.generated.h"

/**
 * 「長押しで置く位置を表示、離して置く」ガジェットの能力(投げずに、決まった距離に置く)。
 *
 * 置く位置 = 視線の先、Max Range(Sava|Aim|Surface)の距離。
 * 地面がなくても空中に置ける。途中に壁があれば、壁の手前(Wall Offset だけ離す)に置く。
 *
 * 狙う・確定・サーバーへの送信・個数の消費は親(USavaHoldAimAbility)が行う。
 * このクラスがやるのは次の 3 つだけ:
 *     ComputeAim   : 置く位置を計算する(自分の画面とサーバーの両方)
 *     OnAimUpdated : 置く位置と範囲を表示する(自分の画面だけ・毎フレーム)
 *     OnConfirmed  : PlaceClass を Spawn する(サーバーだけ)
 *
 * 所持数は Max Charges(既定 3)。置いたときだけ 1 減り、キャンセルでは減らない。
 * キャンセル: 狙っている間に Cancel Input Tag のボタン(既定は右クリック = InputTag.Weapon.Aim)
 */
UCLASS()
class SAVA_API UGadgetPlaceAbility : public USavaHoldAimAbility
{
	GENERATED_BODY()

public:
	UGadgetPlaceAbility();

	/**
	 * 置く位置を計算する。
	 *
	 *     HitDistance < 0(途中に壁がない) : 視点から Reach の位置
	 *     HitDistance >= 0(壁がある)      : 壁から WallOffset 手前(視点より後ろにはしない)
	 *
	 * 計算だけの関数(static)なので、テストから直接呼べる。
	 */
	static FVector CalculatePlaceLocation(
		const FVector& ViewLocation,
		const FVector& Direction,
		float Reach,
		float HitDistance,
		float WallOffset);

protected:
	virtual bool ComputeAim(FTransform& OutTransform, TArray<FVector>& OutPathPoints) const override;

	virtual void OnAimUpdated_Implementation(
		const FTransform& AimTransform,
		bool bIsValid,
		const TArray<FVector>& PathPoints) override;

	virtual void OnConfirmed_Implementation(
		const FTransform& TargetTransform) override;

	// 置く物(例: BP_Gadget_SlowField)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gadget")
	TSubclassOf<AActor> PlaceClass;

	/**
	 * 壁の手前に置くときに、壁から離す距離。
	 *
	 * 0 だと中心が壁に埋まる。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gadget", meta = (ClampMin = "0", ForceUnits = "cm"))
	float WallOffset = 30.0f;

	/**
	 * 置く位置と範囲を簡易表示(デバッグ線)するか。
	 *
	 * 見た目をちゃんと作るときは false にして、Blueprint で On Aim Updated を実装する。
	 * デバッグ線は配布用(Shipping)ビルドでは表示されない。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gadget|Preview")
	bool bDrawDebugPreview = true;

private:
	// 表示する範囲の半径(PlaceClass がスローエリアならその半径。それ以外は 0)
	float GetPreviewRadius() const;
};

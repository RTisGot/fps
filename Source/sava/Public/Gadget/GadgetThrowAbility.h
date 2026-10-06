#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/SavaHoldAimAbility.h"
#include "GadgetThrowAbility.generated.h"

class AGadgetBase;

/**
 * 「長押しで狙い(予測線を表示)、離して投げる」ガジェットの能力。
 *
 * 狙う・確定・サーバーへの送信・個数の消費は親(USavaHoldAimAbility)が行う。
 * このクラスがやるのは次の 3 つだけ:
 *     OnAimStarted : 予測線の速さ・太さを、投げる物(GadgetClass)に合わせる
 *     OnAimUpdated : 予測線と爆発範囲を表示する(自分の画面だけ・毎フレーム)
 *     OnConfirmed  : 投げる物を Spawn して投げる(サーバーだけ)
 *
 * 所持数は Max Charges(既定 2)。投げたときだけ 1 減り、キャンセルでは減らない。
 * キャンセル: 狙っている間に Cancel Input Tag のボタン(既定は右クリック = InputTag.Weapon.Aim)
 */
UCLASS()
class SAVA_API UGadgetThrowAbility : public USavaHoldAimAbility
{
	GENERATED_BODY()

public:
	UGadgetThrowAbility();

protected:
	virtual void OnAimStarted_Implementation() override;

	virtual void OnAimUpdated_Implementation(
		const FTransform& AimTransform,
		bool bIsValid,
		const TArray<FVector>& PathPoints) override;

	virtual void OnConfirmed_Implementation(
		const FTransform& TargetTransform) override;

	// 投げる物(AGadgetBase の子クラス。例: BP_Gadget_FragGrenade)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gadget")
	TSubclassOf<AGadgetBase> GadgetClass;

	/**
	 * 予測線を簡易表示(デバッグ線)するか。
	 *
	 * 見た目をちゃんと作るときは false にして、Blueprint で On Aim Updated を実装する。
	 * デバッグ線は配布用(Shipping)ビルドでは表示されない。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gadget|Preview")
	bool bDrawDebugPreview = true;

private:
	// GadgetClass の設定(投げる速さ・当たり判定の太さ)を予測線の計算に使う値へ写す
	void SyncAimSettingsFromGadget();
};

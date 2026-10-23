#pragma once

#include "CoreMinimal.h"
#include "Gadget/GadgetBase.h"
#include "FragGrenade.generated.h"

/**
 * 着弾した瞬間に爆発するグレネード。
 *
 * 爆発の中心から遠いほどダメージが減る。
 *
 * 調整できる値:
 *     Gadget Data > Damage        … 中心での最大ダメージ
 *     Gadget Data > Effect Radius … 爆発の半径(これより外はダメージなし)
 *     Frag Grenade > Full Damage Radius / Min Damage Ratio … 減り方
 *     所持数は投げる能力(UGadgetThrowAbility)の Max Charges
 *
 * 処理はすべてサーバーで行う(ダメージはサーバーが決める)。
 */
UCLASS()
class SAVA_API AFragGrenade : public AGadgetBase
{
	GENERATED_BODY()

public:
	AFragGrenade();

	/**
	 * 距離からダメージを計算する。
	 *
	 *     0 ～ FullDamageRadius      : MaxDamage
	 *     FullDamageRadius ～ Radius : MaxDamage から MaxDamage * MinDamageRatio まで直線的に減る
	 *     Radius より外              : 0
	 *
	 * 計算だけの関数(static)なので、テストから直接呼べる。
	 */
	static float CalculateFalloffDamage(
		float Distance,
		float MaxDamage,
		float Radius,
		float FullDamageRadius,
		float MinDamageRatio);

protected:
	virtual void OnGadgetLanded(const FHitResult& Hit) override;
	virtual void ApplyGadgetEffect() override;

	/**
	 * 爆発の見た目・音(全員の画面で呼ばれる)。
	 *
	 * Blueprint の子クラスで Spawn System at Location(Niagara)や
	 * Play Sound at Location を置く。
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Gadget|Frag")
	void OnExploded(const FVector& ExplosionLocation);

	//========================================
	// 設定
	//========================================

	/**
	 * この距離までは最大ダメージ(減らない)。
	 *
	 * 足元に落ちたときに確実に最大ダメージにするための範囲。
	 */
	UPROPERTY(
		EditDefaultsOnly,
		Category = "Gadget|Frag",
		meta = (DisplayName = "Full Damage Radius", ClampMin = "0", ForceUnits = "cm"))
	float m_FullDamageRadius = 100.0f;

	/**
	 * 爆発の端(Effect Radius ちょうど)でのダメージの割合。
	 *
	 * 0.2 なら、端では最大ダメージの 20%。
	 */
	UPROPERTY(
		EditDefaultsOnly,
		Category = "Gadget|Frag",
		meta = (DisplayName = "Min Damage Ratio", ClampMin = "0", ClampMax = "1"))
	float m_MinDamageRatio = 0.2f;

	/**
	 * 投げた本人もダメージを受けるか。
	 *
	 * 味方にはダメージを与えない(フレンドリーファイアなし)。
	 */
	UPROPERTY(
		EditDefaultsOnly,
		Category = "Gadget|Frag",
		meta = (DisplayName = "Damage Owner"))
	bool m_bDamageOwner = true;

	/**
	 * 壁越しにはダメージを与えないか。
	 *
	 * 爆発の中心から相手の中心まで、壁(WorldStatic)に遮られていたらダメージなし。
	 */
	UPROPERTY(
		EditDefaultsOnly,
		Category = "Gadget|Frag",
		meta = (DisplayName = "Blocked By Walls"))
	bool m_bBlockedByWalls = true;

	/**
	 * 爆発の範囲とダメージを画面に表示する(調整用)。
	 *
	 * サーバーの画面(リッスンサーバーのホスト・一人プレイ)にだけ表示される。
	 */
	UPROPERTY(
		EditDefaultsOnly,
		Category = "Gadget|Frag|Debug",
		meta = (DisplayName = "Draw Debug"))
	bool m_bDrawDebug = false;

private:
	/**
	 * 爆発の見た目を全員の画面で出す。
	 *
	 * この後すぐ Destroy されるので、必ず届く Reliable にする。
	 */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayExplosion(const FVector_NetQuantize& ExplosionLocation);

	bool CanDamageTarget(const AActor* Target) const;
	bool IsBlockedByWall(const FVector& From, const AActor* Target) const;
};

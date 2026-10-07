#pragma once

#include "CoreMinimal.h"
#include "Gadget/GadgetBase.h"
#include "StunGrenade.generated.h"

class UGameplayEffect;

/**
 * 着弾後に起爆し、範囲内の敵プレイヤーにスタン効果を与えるグレネード。
 *
 * 効果:
 *     ・画面をぼやけさせる
 *     ・ダッシュ不可
 *     ・音の効果は後で追加
 *
 * 処理はサーバーで行う。
 *
 * 調整できる値:
 *     Gadget Data > Effect Radius
 *         … スタン効果が届く範囲
 *
 *     Gadget Data > Fuse Time
 *         … 着弾してから起爆するまでの時間
 *
 *     Stun Grenade > Stun Duration
 *         … スタン状態の持続時間
 *
 * 所持数:
 *     UGadgetThrowAbility の Max Charges = 3
 */
UCLASS()
class SAVA_API AStunGrenade : public AGadgetBase
{
	GENERATED_BODY()

public:

	AStunGrenade();

protected:

	/**
	 * 着地時の処理。
	 *
	 * スタングレネードは着地後、FuseTime待機してから起爆する。
	 */
	virtual void OnGadgetLanded(const FHitResult& Hit) override;

	/**
	 * 起爆時のスタン効果。
	 *
	 * EffectRadius内の敵プレイヤーを検索し、
	 * スタン状態を適用する。
	 */
	virtual void ApplyGadgetEffect() override;

	/**
	 * スタンの視覚効果。
	 *
	 * 後からBlueprintでNiagaraなどを追加する。
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Gadget|Stun")
	void OnStunExploded(const FVector& ExplosionLocation);

	//========================================
	// 設定
	//========================================


	/**
	* スタン時に相手へ付ける GameplayEffect(GE_Stun)。
	* スタンの秒数は GE 側の Duration で決める。
	*/
		UPROPERTY(
			EditDefaultsOnly,
			Category = "Gadget|Stun",
			meta = (DisplayName = "Stun Effect"))
		TSubclassOf<UGameplayEffect> m_StunEffect;

	/**
	 * 投げた本人にもスタン効果を与えるか。
	 */
	UPROPERTY(
		EditDefaultsOnly,
		Category = "Gadget|Stun",
		meta = (
			DisplayName = "Affect Owner"))
	bool m_bAffectOwner = false;

	/**
	 * 味方にもスタン効果を与えるか。
	 *
	 * 現在の仕様では不明なので、調整可能にしている。
	 */
	UPROPERTY(
		EditDefaultsOnly,
		Category = "Gadget|Stun",
		meta = (
			DisplayName = "Affect Allies"))
	bool m_bAffectAllies = false;

	/**
	 * 壁越しにスタン効果を与えないか。
	 *
	 * trueの場合、爆発地点から対象までをLine Traceして
	 * WorldStaticに遮られている対象には効果を与えない。
	 */
	UPROPERTY(
		EditDefaultsOnly,
		Category = "Gadget|Stun",
		meta = (
			DisplayName = "Blocked By Walls"))
	bool m_bBlockedByWalls = true;

	/**
	 * 起爆時のデバッグ表示。
	 *
	 * 調整中のみ使用する。
	 */
	UPROPERTY(
		EditDefaultsOnly,
		Category = "Gadget|Stun|Debug",
		meta = (
			DisplayName = "Draw Debug"))
	bool m_bDrawDebug = false;

private:

	/**
	 * スタン対象として有効か判定する。
	 */
	bool CanAffectTarget(const AActor* Target) const;

	/**
	 * 爆発地点から対象までの間に壁があるか判定する。
	 */
	bool IsBlockedByWall(
		const FVector& From,
		const AActor* Target) const;

	/**
	 * 全クライアントへスタン起爆の視覚効果を通知する。
	 */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayStunExplosion(
		const FVector_NetQuantize& ExplosionLocation);

	/**
	 * 着地後の起爆タイマー。
	 */
	FTimerHandle m_DetonationTimerHandle;
};
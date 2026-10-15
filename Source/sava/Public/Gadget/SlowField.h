#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameFramework/Actor.h"
#include "SlowField.generated.h"

class APlayerState;
class UAbilitySystemComponent;
class UGameplayEffect;
class USphereComponent;

/**
 * 空中にも置ける球体のスローエリア。
 *
 * 中にいる敵の移動速度を下げる(出ると元に戻る)。
 * 投げ物ではなく、設置する能力(UGadgetPlaceAbility)が目の前に置く。
 *
 * ・当たり判定は持たない(人も弾も通り抜ける)
 * ・Duration 秒たつと消える(投げ物より長く残る)
 * ・減速は GameplayEffect(既定は USavaGE_Slowed)で付け、
 *   CharacterMovement が MoveSpeedMultiplier を速度に掛ける
 * ・スローエリアが重なっても減速は重ならない(State.Slowed が付いている人には効かない)
 *
 * 調整できる値(Blueprint の子クラス BP_Gadget_SlowField などで変える):
 *     Slow Field > Radius                … 球の半径
 *     Slow Field > Duration              … 残る時間
 *     Slow Field > Move Speed Multiplier … 中にいる間の速度の倍率(0.5 = 半分)
 *     所持数は設置する能力(UGadgetPlaceAbility)の Max Charges
 *
 * 判定(減速を付ける・外す)はサーバーだけが行う。
 */
UCLASS()
class SAVA_API ASlowField : public AActor
{
	GENERATED_BODY()

public:
	ASlowField();

	UFUNCTION(BlueprintPure, Category = "Gadget|SlowField")
	float GetRadius() const
	{
		return m_Radius;
	}

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * 置かれたとき(全員の画面で呼ばれる)。
	 *
	 * Blueprint の子クラスで Niagara・音を置く。半径は Get Radius で取れる。
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Gadget|SlowField")
	void OnSlowFieldStarted();

	/**
	 * 消えるとき(全員の画面で呼ばれる)。
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Gadget|SlowField")
	void OnSlowFieldEnded();

	//========================================
	// Components
	//========================================

	/**
	 * 範囲の表示用(エディタで大きさが見える)。当たり判定は持たない。
	 */
	UPROPERTY(VisibleAnywhere, Category = "Gadget|SlowField")
	TObjectPtr<USphereComponent> m_AreaComponent;

	//========================================
	// 設定
	//========================================

	UPROPERTY(
		EditDefaultsOnly,
		Category = "Gadget|SlowField",
		meta = (DisplayName = "Radius", ClampMin = "1", ForceUnits = "cm"))
	float m_Radius = 400.0f;

	/**
	 * 残る時間。0 以下なら消えない(調整用)。
	 */
	UPROPERTY(
		EditDefaultsOnly,
		Category = "Gadget|SlowField",
		meta = (DisplayName = "Duration", ForceUnits = "s"))
	float m_Duration = 15.0f;

	/**
	 * 中にいる間の移動速度の倍率。0.5 なら半分の速さになる。
	 */
	UPROPERTY(
		EditDefaultsOnly,
		Category = "Gadget|SlowField",
		meta = (DisplayName = "Move Speed Multiplier", ClampMin = "0", ClampMax = "1"))
	float m_MoveSpeedMultiplier = 0.5f;

	/**
	 * 中にいる人を調べ直す間隔。短いほど出入りの反応が早い。
	 */
	UPROPERTY(
		EditDefaultsOnly,
		Category = "Gadget|SlowField",
		meta = (DisplayName = "Refresh Interval", ClampMin = "0.02", ForceUnits = "s"))
	float m_RefreshInterval = 0.1f;

	/**
	 * 中にいる間に付ける GameplayEffect。
	 *
	 * SetByCaller.MoveSpeedMultiplier に Move Speed Multiplier が渡される。
	 * 既定(USavaGE_Slowed)のままでよい。別の効果も付けたいときだけ変える。
	 */
	UPROPERTY(
		EditDefaultsOnly,
		Category = "Gadget|SlowField",
		meta = (DisplayName = "Slow Effect"))
	TSubclassOf<UGameplayEffect> m_SlowEffect;

	/**
	 * 置いた本人も遅くなるか。
	 */
	UPROPERTY(
		EditDefaultsOnly,
		Category = "Gadget|SlowField",
		meta = (DisplayName = "Affect Owner"))
	bool m_bAffectOwner = false;

	/**
	 * 味方も遅くなるか。
	 */
	UPROPERTY(
		EditDefaultsOnly,
		Category = "Gadget|SlowField",
		meta = (DisplayName = "Affect Allies"))
	bool m_bAffectAllies = false;

	/**
	 * 範囲を球の線で表示する(見た目を作るまでの仮表示)。
	 *
	 * 全員の画面に表示される。配布用(Shipping)ビルドでは表示されない。
	 */
	UPROPERTY(
		EditDefaultsOnly,
		Category = "Gadget|SlowField|Debug",
		meta = (DisplayName = "Draw Debug"))
	bool m_bDrawDebug = true;

private:
	/**
	 * 範囲内の人を調べ直し、入った人に減速を付け、出た人から外す(サーバーのみ)。
	 */
	void RefreshTargets();

	void ApplySlow(UAbilitySystemComponent* TargetASC);
	void RemoveAllSlows();

	bool CanAffectTarget(const AActor* Target) const;

	/**
	 * 置いた人の PlayerState。
	 *
	 * 置いた人が死んでも味方判定ができるように、置いたときに覚えておく。
	 */
	TWeakObjectPtr<APlayerState> m_OwnerPlayerState;

	/**
	 * 今このエリアが減速を付けている相手と、その効果。
	 *
	 * ASC は PlayerState にあり、死んでも残る。外し忘れると次の命でも遅いままになるので、
	 * キャラクターではなく ASC ごとに覚えて必ず外す。
	 */
	TMap<TWeakObjectPtr<UAbilitySystemComponent>, FActiveGameplayEffectHandle> m_SlowedTargets;

	FTimerHandle m_RefreshTimerHandle;
};

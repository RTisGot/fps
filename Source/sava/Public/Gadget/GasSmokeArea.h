#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SphereComponent.h"
#include "GasSmokeArea.generated.h"

UCLASS()
class SAVA_API AGasSmokeArea : public AActor
{
	GENERATED_BODY()

public:
	AGasSmokeArea();

	/**
	 * ガスエリアを初期化する。
	 *
	 * @param InOwner ガジェット使用者
	 * @param InRadius 効果範囲
	 * @param InDuration 効果時間
	 * @param InDamagePerSecond 毎秒ダメージ
	 */
	void InitializeGas(
		AActor* InOwner,
		float InRadius,
		float InDuration,
		float InDamagePerSecond);

protected:
	virtual void BeginPlay() override;

	/**
	 * ガス煙開始時の演出。
	 * Blueprint側でVFX/SFXなどを設定する。
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Gadget|GasSmoke")
	void OnGasSmokeStarted();

	/**
	 * ガス煙終了時の演出。
	 * Blueprint側でVFX/SFXなどを設定する。
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Gadget|GasSmoke")
	void OnGasSmokeEnded();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gadget|GasSmoke")
	TObjectPtr<USphereComponent> m_EffectCollisionComponent;

	/**
	 * ダメージを与える間隔。
	 * 1秒なら3 DPSの場合、1秒ごとに3ダメージ。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gadget|GasSmoke",
		meta = (DisplayName = "Damage Interval", ClampMin = "0.01", ForceUnits = "s"))
	float m_DamageInterval = 1.0f;

	/**
	 * ガス使用者本人にもダメージを与えるか。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gadget|GasSmoke",
		meta = (DisplayName = "Affect Owner"))
	bool m_bAffectOwner = true;

	/**
	 * 味方にもダメージを与えるか。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gadget|GasSmoke",
		meta = (DisplayName = "Affect Allies"))
	bool m_bAffectAllies = false;

private:

	/**
	 * 現在のガス範囲内にいる対象へダメージを与える。
	 */
	void ApplyGasDamage();

	/**
	 * ガス煙を終了する。
	 */
	void EndGasSmoke();

	/**
	 * 対象にガスダメージを与えてよいか判定する。
	 */
	bool CanDamageTarget(const AActor* Target) const;

	UPROPERTY()
	TObjectPtr<AActor> m_GasOwner;

	float m_EffectRadius = 0.0f;
	float m_EffectDuration = 0.0f;
	float m_DamagePerSecond = 0.0f;

	FTimerHandle m_DamageTimerHandle;
	FTimerHandle m_EndTimerHandle;
};
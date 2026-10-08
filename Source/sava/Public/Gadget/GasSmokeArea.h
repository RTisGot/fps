#pragma once

#include "CoreMinimal.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Actor.h"
#include "GasSmokeArea.generated.h"

UCLASS()
class SAVA_API AGasSmokeArea : public AActor
{
	GENERATED_BODY()

public:
	AGasSmokeArea();

	/**
	 * ガス煙の設定を初期化する。
	 */
	void InitializeGas(
		AActor* InOwner,
		float InRadius,
		float InDuration,
		float InDamagePerSecond,
		bool bInAffectOwner);

protected:
	virtual void BeginPlay() override;

	/**
	 * ガス煙開始時の演出。
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Gadget|GasSmoke")
	void OnGasSmokeStarted();

	/**
	 * ガス煙終了時の演出。
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Gadget|GasSmoke")
	void OnGasSmokeEnded();

	/**
	 * ガス煙の効果範囲。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gadget|GasSmoke")
	TObjectPtr<USphereComponent> m_EffectCollisionComponent;

	/**
	 * ダメージを与える間隔。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gadget|GasSmoke",
		meta = (
			DisplayName = "Damage Interval",
			ClampMin = "0.01",
			ForceUnits = "s"))
	float m_DamageInterval = 1.0f;

private:
	/**
	 * ガス煙内の対象へダメージを与える。
	 */
	void ApplyGasDamage();

	/**
	 * ガス煙を終了する。
	 */
	void EndGasSmoke();

	/**
	 * 対象へダメージを与えてよいか判定する。
	 */
	bool CanDamageTarget(const AActor* Target) const;

	/**
	 * ガス煙の所有者。
	 */
	UPROPERTY()
	TObjectPtr<AActor> m_GasOwner;

	/**
	 * ガス煙の効果範囲。
	 */
	float m_EffectRadius = 0.0f;

	/**
	 * ガス煙の効果時間。
	 */
	float m_EffectDuration = 0.0f;

	/**
	 * 1秒あたりのダメージ量。
	 */
	float m_DamagePerSecond = 0.0f;

	/**
	 * 自分自身へダメージを与えるか。
	 */
	bool m_bAffectOwner = false;

	/**
	 * ダメージタイマー。
	 */
	FTimerHandle m_DamageTimerHandle;

	/**
	 * 終了タイマー。
	 */
	FTimerHandle m_EndTimerHandle;
};
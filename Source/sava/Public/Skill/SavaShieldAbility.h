#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/SavaGameplayAbility.h"
#include "SavaShieldAbility.generated.h"

class ASavaDomeShield;

/**
 * スキル「シールド」。ボタンを 1 回押すと、自分の足元を中心にドーム型のシールドを展開する。
 *
 * ・展開するのはサーバーだけ(シールドは全員へ同期される)
 * ・押した瞬間にクールダウンが始まる(Cooldown Duration)
 * ・シールドの大きさ・時間は Shield Class(ASavaDomeShield の子クラス)側で決める
 *
 * 使い方:
 *     USavaSkillData のデータアセットを作り、Ability にこのクラス(か Blueprint の子クラス)、
 *     Input Tag に InputTag.Skill を入れる。
 */
UCLASS()
class SAVA_API USavaShieldAbility : public USavaGameplayAbility
{
	GENERATED_BODY()

public:
	USavaShieldAbility();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	// 展開するシールド(例: BP_DomeShield)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shield")
	TSubclassOf<ASavaDomeShield> ShieldClass;

	// 新しく展開したとき、自分が前に出したシールドを消すか
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shield")
	bool bReplaceExistingShield = true;

private:
	void SpawnShield(AActor* Avatar) const;
};

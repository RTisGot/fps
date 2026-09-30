// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Weapon/SavaWeaponAbility.h"
#include "Weapon/SavaWeaponTypes.h"
#include "SavaWeaponReloadAbility.generated.h"

//リロード。Reload Time 待ってから弾を込める(自分の画面とサーバーの両方で待ち、両方で弾数を更新する)
//・リロード中は State.Weapon.Reloading タグが付き、撃てない
//・始めると射撃を止める。持ち替え・死亡で中断した場合は弾を込めない
//・マガジンが満タン、または予備弾がないときは発動しない
UCLASS()
class SAVA_API USavaWeaponReloadAbility : public USavaWeaponAbility
{
	GENERATED_BODY()

public:
	USavaWeaponReloadAbility();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	//リロードを始めた(自分の画面だけ)。Reload Time はリロードにかかる秒数(アニメーションの速さ合わせに使う)
	UFUNCTION(BlueprintImplementableEvent, Category = "Sava|Weapon")
	void OnReloadStarted(float ReloadTime);

	//リロードが終わった(自分の画面だけ)。bCompleted = 弾を込めたか / false なら中断
	UFUNCTION(BlueprintImplementableEvent, Category = "Sava|Weapon")
	void OnReloadEnded(bool bCompleted);

private:
	UFUNCTION()
	void OnReloadTimeElapsed();

	void StopReloadMontage();

	//リロードしている枠
	ESavaWeaponSlot ReloadSlot = ESavaWeaponSlot::Primary;

	bool bReloadCompleted = false;
};

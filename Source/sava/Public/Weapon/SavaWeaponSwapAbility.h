// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Weapon/SavaWeaponAbility.h"
#include "SavaWeaponSwapAbility.generated.h"

//メイン武器とサブ武器を持ち替える(自分の画面では押した瞬間に切り替わる)
//持ち替えると、他の武器の能力(射撃・ADS・リロード)を止める
UCLASS()
class SAVA_API USavaWeaponSwapAbility : public USavaWeaponAbility
{
	GENERATED_BODY()

public:
	USavaWeaponSwapAbility();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};

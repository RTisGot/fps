// Fill out your copyright notice in the Description page of Project Settings.

#include "Weapon/SavaWeaponAimAbility.h"
#include "SavaGameplayTags.h"

USavaWeaponAimAbility::USavaWeaponAimAbility()
{
	//離したら自動で終わる(親クラスの InputReleased)
	ActivationPolicy = ESavaAbilityActivationPolicy::WhileInputActive;

	ActivationOwnedTags.AddTag(SavaGameplayTags::State_Weapon_Aiming);
}

void USavaWeaponAimAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	//発動している間 State.Weapon.Aiming が付くので、ここでは何もしない
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}

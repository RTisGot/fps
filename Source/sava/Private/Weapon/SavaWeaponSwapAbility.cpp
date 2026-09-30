// Fill out your copyright notice in the Description page of Project Settings.

#include "Weapon/SavaWeaponSwapAbility.h"
#include "Weapon/SavaEquipmentComponent.h"
#include "SavaGameplayTags.h"

USavaWeaponSwapAbility::USavaWeaponSwapAbility()
{
	ActivationPolicy = ESavaAbilityActivationPolicy::OnInputTriggered;

	//この能力自身は止めない(発動した能力は Cancel の対象から外れる)
	CancelAbilitiesWithTag.AddTag(SavaGameplayTags::Ability_Type_Weapon);
}

bool USavaWeaponSwapAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const USavaEquipmentComponent* Equipment = GetEquipmentFromActorInfo(ActorInfo);
	return Equipment && Equipment->CanSwapWeapon();
}

void USavaWeaponSwapAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	//自分の画面とサーバーの両方で持ち替える
	if (USavaEquipmentComponent* Equipment = GetEquipmentFromActorInfo(ActorInfo))
	{
		Equipment->SwapWeapon();
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

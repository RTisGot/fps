// Fill out your copyright notice in the Description page of Project Settings.

#include "Weapon/SavaWeaponAbility.h"
#include "Weapon/SavaEquipmentComponent.h"
#include "Weapon/SavaWeaponData.h"
#include "SavaGameplayTags.h"

USavaWeaponAbility::USavaWeaponAbility()
{
	AddAssetTag(SavaGameplayTags::Ability_Type_Weapon);
}

void USavaWeaponAbility::AddAssetTag(const FGameplayTag& Tag)
{
	FGameplayTagContainer Tags = GetAssetTags();
	Tags.AddTag(Tag);
	SetAssetTags(Tags);
}

bool USavaWeaponAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const USavaEquipmentComponent* Equipment = GetEquipmentFromActorInfo(ActorInfo);
	return Equipment && Equipment->GetCurrentWeaponData();
}

USavaEquipmentComponent* USavaWeaponAbility::GetEquipmentComponent() const
{
	return GetEquipmentFromActorInfo(GetCurrentActorInfo());
}

USavaWeaponData* USavaWeaponAbility::GetCurrentWeaponData() const
{
	const USavaEquipmentComponent* Equipment = GetEquipmentComponent();
	return Equipment ? Equipment->GetCurrentWeaponData() : nullptr;
}

USavaEquipmentComponent* USavaWeaponAbility::GetEquipmentFromActorInfo(const FGameplayAbilityActorInfo* ActorInfo)
{
	return ActorInfo ? USavaEquipmentComponent::FindEquipmentComponent(ActorInfo->AvatarActor.Get()) : nullptr;
}

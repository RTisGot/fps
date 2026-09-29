// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/SavaAbilitySet.h"
#include "AbilitySystem/SavaGameplayAbility.h"
#include "AbilitySystemComponent.h"

void FSavaAbilitySet_GrantedHandles::TakeFromAbilitySystem(UAbilitySystemComponent* AbilitySystemComponent)
{
	if (!AbilitySystemComponent || !AbilitySystemComponent->IsOwnerActorAuthoritative())
	{
		return;
	}

	for (const FGameplayAbilitySpecHandle& Handle : AbilitySpecHandles)
	{
		if (Handle.IsValid())
		{
			AbilitySystemComponent->ClearAbility(Handle);
		}
	}

	for (const FActiveGameplayEffectHandle& Handle : GameplayEffectHandles)
	{
		if (Handle.IsValid())
		{
			AbilitySystemComponent->RemoveActiveGameplayEffect(Handle);
		}
	}

	AbilitySpecHandles.Reset();
	GameplayEffectHandles.Reset();
}

void USavaAbilitySet::GiveToAbilitySystem(UAbilitySystemComponent* AbilitySystemComponent, FSavaAbilitySet_GrantedHandles* OutGrantedHandles) const
{
	if (!AbilitySystemComponent || !AbilitySystemComponent->IsOwnerActorAuthoritative())
	{
		return;
	}

	for (const FSavaAbilitySet_GameplayAbility& Entry : GrantedGameplayAbilities)
	{
		if (!Entry.Ability)
		{
			continue;
		}

		//ボタンのタグを能力に持たせておき、入力時にこのタグで探す
		FGameplayAbilitySpec Spec(Entry.Ability->GetDefaultObject<USavaGameplayAbility>(), Entry.AbilityLevel);
		if (Entry.InputTag.IsValid())
		{
			Spec.GetDynamicSpecSourceTags().AddTag(Entry.InputTag);
		}

		const FGameplayAbilitySpecHandle Handle = AbilitySystemComponent->GiveAbility(Spec);
		if (OutGrantedHandles)
		{
			OutGrantedHandles->AbilitySpecHandles.Add(Handle);
		}
	}

	for (const FSavaAbilitySet_GameplayEffect& Entry : GrantedGameplayEffects)
	{
		if (!Entry.GameplayEffect)
		{
			continue;
		}

		const FActiveGameplayEffectHandle Handle = AbilitySystemComponent->ApplyGameplayEffectToSelf(
			Entry.GameplayEffect->GetDefaultObject<UGameplayEffect>(), Entry.EffectLevel, AbilitySystemComponent->MakeEffectContext());
		if (OutGrantedHandles)
		{
			OutGrantedHandles->GameplayEffectHandles.Add(Handle);
		}
	}
}

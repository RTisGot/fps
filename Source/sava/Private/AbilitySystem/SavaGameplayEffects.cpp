// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/SavaGameplayEffects.h"
#include "AbilitySystem/SavaAttributeSet.h"
#include "SavaGameplayTags.h"

namespace
{
	FGameplayModifierInfo MakeSetByCallerAddModifier(const FGameplayAttribute& Attribute, const FGameplayTag& DataTag)
	{
		FSetByCallerFloat SetByCaller;
		SetByCaller.DataTag = DataTag;

		FGameplayModifierInfo Modifier;
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = EGameplayModOp::Additive;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
		return Modifier;
	}
}

USavaGE_Damage::USavaGE_Damage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	Modifiers.Add(MakeSetByCallerAddModifier(USavaAttributeSet::GetDamageAttribute(), SavaGameplayTags::SetByCaller_Damage));
}

USavaGE_Healing::USavaGE_Healing()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	Modifiers.Add(MakeSetByCallerAddModifier(USavaAttributeSet::GetHealingAttribute(), SavaGameplayTags::SetByCaller_Healing));
}

USavaGE_Cooldown::USavaGE_Cooldown()
{
	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = SavaGameplayTags::SetByCaller_Cooldown;

	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
}

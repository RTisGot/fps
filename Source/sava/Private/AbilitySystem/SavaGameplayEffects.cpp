// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/SavaGameplayEffects.h"
#include "AbilitySystem/SavaAttributeSet.h"
#include "SavaGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

namespace
{
	//付いている間、相手にタグを付ける(GE の Components の "Grant Tags to Target Actor" と同じ)
	UTargetTagsGameplayEffectComponent* MakeGrantedTagComponent(UGameplayEffect& Effect, const FGameplayTag& Tag)
	{
		UTargetTagsGameplayEffectComponent* TagsComponent = Effect.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
		FInheritedTagContainer Tags;
		Tags.Added.AddTag(Tag);
		TagsComponent->SetAndApplyTargetTagChanges(Tags);
		return TagsComponent;
	}

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

USavaGE_GadgetChargeCost::USavaGE_GadgetChargeCost()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo Modifier;
	Modifier.Attribute = USavaAttributeSet::GetGadgetChargesAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(-1.0f));
	Modifiers.Add(Modifier);
}

USavaGE_CarryingFlag::USavaGE_CarryingFlag(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = SavaGameplayTags::SetByCaller_MoveSpeedMultiplier;

	FGameplayModifierInfo Modifier;
	Modifier.Attribute = USavaAttributeSet::GetMoveSpeedMultiplierAttribute();
	Modifier.ModifierOp = EGameplayModOp::MultiplyCompound;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
	Modifiers.Add(Modifier);

	GEComponents.Add(MakeGrantedTagComponent(*this, SavaGameplayTags::State_CarryingFlag));
}

USavaGE_RoundFrozen::USavaGE_RoundFrozen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	FGameplayModifierInfo Modifier;
	Modifier.Attribute = USavaAttributeSet::GetMoveSpeedMultiplierAttribute();
	Modifier.ModifierOp = EGameplayModOp::Override;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.0f));
	Modifiers.Add(Modifier);

	GEComponents.Add(MakeGrantedTagComponent(*this, SavaGameplayTags::State_RoundFrozen));
}

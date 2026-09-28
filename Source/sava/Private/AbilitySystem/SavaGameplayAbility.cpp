// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/SavaGameplayAbility.h"
#include "AbilitySystem/SavaGameplayEffects.h"
#include "AbilitySystemComponent.h"
#include "SavaCharacterMovementComponent.h"
#include "SavaGameplayTags.h"
#include "savaCharacter.h"

USavaGameplayAbility::USavaGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	ActivationBlockedTags.AddTag(SavaGameplayTags::State_Dead);
}

AsavaCharacter* USavaGameplayAbility::GetSavaCharacterFromActorInfo() const
{
	return Cast<AsavaCharacter>(GetAvatarActorFromActorInfo());
}

USavaCharacterMovementComponent* USavaGameplayAbility::GetSavaMovementFromActorInfo() const
{
	const AsavaCharacter* Character = GetSavaCharacterFromActorInfo();
	return Character ? Character->GetSavaCharacterMovementComponent() : nullptr;
}

const FGameplayTagContainer* USavaGameplayAbility::GetCooldownTags() const
{
	FGameplayTagContainer* MutableTags = const_cast<FGameplayTagContainer*>(&TempCooldownTags);
	MutableTags->Reset();

	if (const FGameplayTagContainer* ParentTags = Super::GetCooldownTags())
	{
		MutableTags->AppendTags(*ParentTags);
	}
	MutableTags->AppendTags(CooldownTags);
	return MutableTags;
}

void USavaGameplayAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	const float Duration = CooldownDuration.GetValueAtLevel(GetAbilityLevel(Handle, ActorInfo));
	if (Duration <= 0.0f || CooldownTags.IsEmpty())
	{
		//独自のクールダウン用 GameplayEffect を Cooldown Gameplay Effect Class に設定した場合はそちらを使う
		Super::ApplyCooldown(Handle, ActorInfo, ActivationInfo);
		return;
	}

	//共通のクールダウン GE に、秒数とこの能力専用のタグを載せて自分に付ける
	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo, USavaGE_Cooldown::StaticClass(), GetAbilityLevel(Handle, ActorInfo));
	if (SpecHandle.IsValid())
	{
		SpecHandle.Data->DynamicGrantedTags.AppendTags(CooldownTags);
		SpecHandle.Data->SetSetByCallerMagnitude(SavaGameplayTags::SetByCaller_Cooldown, Duration);
		ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
	}
}

void USavaGameplayAbility::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputReleased(Handle, ActorInfo, ActivationInfo);

	if (ActivationPolicy == ESavaAbilityActivationPolicy::WhileInputActive && IsActive())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}

void USavaGameplayAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);

	//パッシブはサーバーで付与された時点で発動する(クライアントへは自動で伝わる)
	if (ActivationPolicy == ESavaAbilityActivationPolicy::OnSpawn && ActorInfo && ActorInfo->IsNetAuthority() && !Spec.IsActive())
	{
		ActorInfo->AbilitySystemComponent->TryActivateAbility(Spec.Handle);
	}
}

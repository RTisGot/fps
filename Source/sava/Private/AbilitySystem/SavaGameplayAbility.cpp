// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/SavaGameplayAbility.h"
#include "AbilitySystem/SavaAttributeSet.h"
#include "AbilitySystem/SavaAbilitySettings.h"
#include "AbilitySystemComponent.h"
#include "SavaCharacterMovementComponent.h"
#include "SavaGameplayTags.h"
#include "savaCharacter.h"

USavaGameplayAbility::USavaGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	ActivationBlockedTags.AddTag(SavaGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(SavaGameplayTags::State_RoundFrozen);
}

bool USavaGameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	//スキルは旗を運んでいる間は使えない(Blueprint で Ability.Type.Skill を付けた能力にも効くように、ここでまとめて判定する)
	const UAbilitySystemComponent* AbilitySystem = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (AbilitySystem
		&& GetAssetTags().HasTag(SavaGameplayTags::Ability_Type_Skill)
		&& AbilitySystem->HasMatchingGameplayTag(SavaGameplayTags::State_CarryingFlag))
	{
		if (OptionalRelevantTags)
		{
			OptionalRelevantTags->AddTag(SavaGameplayTags::State_CarryingFlag);
		}
		return false;
	}
	return true;
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
	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo, USavaAbilitySettings::GetCooldownEffectClass(), GetAbilityLevel(Handle, ActorInfo));
	if (SpecHandle.IsValid())
	{
		SpecHandle.Data->DynamicGrantedTags.AppendTags(CooldownTags);
		SpecHandle.Data->SetSetByCallerMagnitude(SavaGameplayTags::SetByCaller_Cooldown, Duration);
		ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
	}
}

UGameplayEffect* USavaGameplayAbility::GetCostGameplayEffect() const
{
	//個数制の能力は共通のコスト GE(GadgetCharges を 1 減らす)を使う
	if (MaxCharges > 0)
	{
		return USavaAbilitySettings::GetGadgetChargeCostEffectClass()->GetDefaultObject<UGameplayEffect>();
	}
	//個数制でなければ、Cost Gameplay Effect Class に設定したもの(なければコストなし)
	return Super::GetCostGameplayEffect();
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

	//個数制の能力はサーバーで付与された時点で満タンにする(リスポーン時は能力が付与し直されるので、ここで補充される)
	if (MaxCharges > 0 && ActorInfo && ActorInfo->IsNetAuthority())
	{
		if (UAbilitySystemComponent* AbilitySystem = ActorInfo->AbilitySystemComponent.Get())
		{
			//最大値を先に設定する(残り個数は最大値を超えないように制限されるため)
			AbilitySystem->SetNumericAttributeBase(USavaAttributeSet::GetMaxGadgetChargesAttribute(), MaxCharges);
			AbilitySystem->SetNumericAttributeBase(USavaAttributeSet::GetGadgetChargesAttribute(), MaxCharges);
		}
	}

	//パッシブはサーバーで付与された時点で発動する(クライアントへは自動で伝わる)
	if (ActivationPolicy == ESavaAbilityActivationPolicy::OnSpawn && ActorInfo && ActorInfo->IsNetAuthority() && !Spec.IsActive())
	{
		ActorInfo->AbilitySystemComponent->TryActivateAbility(Spec.Handle);
	}
}

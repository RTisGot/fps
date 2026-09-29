// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/SavaAbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/SavaHoldAimAbility.h"

USavaAbilitySystemComponent::USavaAbilitySystemComponent()
{
	SetIsReplicatedByDefault(true);
}

void USavaAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	//狙っている最中の能力のキャンセルボタンだった場合は、キャンセルだけして他の能力は発動しない
	if (CancelAimingAbilities(InputTag))
	{
		return;
	}

	//一覧を走査しながら発動すると一覧が変わる可能性があるので、先に対象を集める
	TArray<FGameplayAbilitySpecHandle> HandlesToActivate;
	{
		ABILITYLIST_SCOPE_LOCK();
		for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
		{
			if (!Spec.Ability || !Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
			{
				continue;
			}

			Spec.InputPressed = true;
			if (Spec.IsActive())
			{
				//発動中の能力へ「押された」を伝える(Blueprint の Wait Input Press ノード用)
				AbilitySpecInputPressed(Spec);
				if (const UGameplayAbility* Instance = Spec.GetPrimaryInstance())
				{
					InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed, Spec.Handle, Instance->GetCurrentActivationInfo().GetActivationPredictionKey());
				}
			}
			else
			{
				HandlesToActivate.Add(Spec.Handle);
			}
		}
	}

	for (const FGameplayAbilitySpecHandle& Handle : HandlesToActivate)
	{
		TryActivateAbility(Handle);
	}
}

bool USavaAbilitySystemComponent::CancelAimingAbilities(const FGameplayTag& InputTag)
{
	TArray<USavaHoldAimAbility*> AbilitiesToCancel;
	{
		ABILITYLIST_SCOPE_LOCK();
		for (const FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
		{
			if (!Spec.IsActive())
			{
				continue;
			}
			for (UGameplayAbility* Instance : Spec.GetAbilityInstances())
			{
				USavaHoldAimAbility* AimAbility = Cast<USavaHoldAimAbility>(Instance);
				if (AimAbility && AimAbility->IsActive() && AimAbility->GetCancelInputTag().MatchesTagExact(InputTag))
				{
					AbilitiesToCancel.Add(AimAbility);
				}
			}
		}
	}

	for (USavaHoldAimAbility* AimAbility : AbilitiesToCancel)
	{
		AimAbility->CancelAiming();
	}
	return AbilitiesToCancel.Num() > 0;
}

void USavaAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	ABILITYLIST_SCOPE_LOCK();
	for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (!Spec.Ability || !Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			continue;
		}

		Spec.InputPressed = false;
		if (Spec.IsActive())
		{
			//発動中の能力へ「離された」を伝える(Blueprint の Wait Input Release ノード用)
			AbilitySpecInputReleased(Spec);
			if (const UGameplayAbility* Instance = Spec.GetPrimaryInstance())
			{
				InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, Spec.Handle, Instance->GetCurrentActivationInfo().GetActivationPredictionKey());
			}
		}
	}
}

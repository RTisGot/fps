// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/SavaAbilitySettings.h"

namespace
{
	template <typename TEffect>
	TSubclassOf<UGameplayEffect> LoadEffectOrDefault(const TSoftClassPtr<TEffect>& Configured)
	{
		//初回だけ読み込む(以降は読み込み済みのクラスが返る)
		if (UClass* Loaded = Configured.LoadSynchronous())
		{
			return Loaded;
		}
		return TEffect::StaticClass();
	}
}

TSubclassOf<UGameplayEffect> USavaAbilitySettings::GetDamageEffectClass()
{
	return LoadEffectOrDefault(GetDefault<USavaAbilitySettings>()->DamageEffect);
}

TSubclassOf<UGameplayEffect> USavaAbilitySettings::GetHealingEffectClass()
{
	return LoadEffectOrDefault(GetDefault<USavaAbilitySettings>()->HealingEffect);
}

TSubclassOf<UGameplayEffect> USavaAbilitySettings::GetCooldownEffectClass()
{
	return LoadEffectOrDefault(GetDefault<USavaAbilitySettings>()->CooldownEffect);
}

TSubclassOf<UGameplayEffect> USavaAbilitySettings::GetGadgetChargeCostEffectClass()
{
	return LoadEffectOrDefault(GetDefault<USavaAbilitySettings>()->GadgetChargeCostEffect);
}

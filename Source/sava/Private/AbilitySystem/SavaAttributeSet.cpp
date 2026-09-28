// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/SavaAttributeSet.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

USavaAttributeSet::USavaAttributeSet()
{
	//初期値(キャラごとの値は、将来 DataTable から初期化用の GameplayEffect で上書きする)
	InitHealth(100.0f);
	InitMaxHealth(100.0f);
	InitMoveSpeedMultiplier(1.0f);
	InitDamage(0.0f);
	InitHealing(0.0f);
}

void USavaAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(USavaAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USavaAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USavaAttributeSet, MoveSpeedMultiplier, COND_None, REPNOTIFY_Always);
}

void USavaAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	//範囲外の値にならないようにする
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
	else if (Attribute == GetMaxHealthAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.0f);
	}
	else if (Attribute == GetMoveSpeedMultiplierAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
}

void USavaAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetDamageAttribute())
	{
		const float LocalDamage = GetDamage();
		SetDamage(0.0f);

		if (LocalDamage > 0.0f)
		{
			SetHealth(FMath::Clamp(GetHealth() - LocalDamage, 0.0f, GetMaxHealth()));
		}
	}
	else if (Data.EvaluatedData.Attribute == GetHealingAttribute())
	{
		const float LocalHealing = GetHealing();
		SetHealing(0.0f);

		if (LocalHealing > 0.0f)
		{
			SetHealth(FMath::Clamp(GetHealth() + LocalHealing, 0.0f, GetMaxHealth()));
		}
	}
	else if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
	}

	//HP が 0 になった瞬間だけ通知する
	if (GetHealth() <= 0.0f && !bOutOfHealth)
	{
		const FGameplayEffectContextHandle& Context = Data.EffectSpec.GetEffectContext();
		OnOutOfHealth.Broadcast(Context.GetOriginalInstigator(), Context.GetEffectCauser(), Data.EvaluatedData.Magnitude);
	}
	bOutOfHealth = GetHealth() <= 0.0f;
}

void USavaAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USavaAttributeSet, Health, OldValue);
}

void USavaAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USavaAttributeSet, MaxHealth, OldValue);
}

void USavaAttributeSet::OnRep_MoveSpeedMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(USavaAttributeSet, MoveSpeedMultiplier, OldValue);
}

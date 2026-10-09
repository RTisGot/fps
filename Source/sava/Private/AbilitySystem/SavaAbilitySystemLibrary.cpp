// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/SavaAbilitySystemLibrary.h"
#include "AbilitySystem/SavaAbilitySettings.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Pawn.h"
#include "Player/SavaPlayerState.h"
#include "SavaGameplayTags.h"

namespace
{
	//Actor の持ち主プレイヤーの PlayerState(弾・設置物は Instigator をたどる)
	const APlayerState* FindOwningPlayerState(const AActor* Actor)
	{
		if (!Actor)
		{
			return nullptr;
		}
		if (const APlayerState* PlayerState = Cast<APlayerState>(Actor))
		{
			return PlayerState;
		}
		if (const APawn* Pawn = Cast<APawn>(Actor))
		{
			if (const APlayerState* PlayerState = Pawn->GetPlayerState())
			{
				return PlayerState;
			}
			//死んだ体は PlayerState との紐付けが外れる(Unpossess)が、ASC の持ち主(= PlayerState)は覚えている
			//(死んだ人が投げたグレネードでも、味方判定とキルの加算ができるように)
			const IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(Pawn);
			const UAbilitySystemComponent* AbilitySystem = AbilitySystemInterface ? AbilitySystemInterface->GetAbilitySystemComponent() : nullptr;
			return AbilitySystem ? Cast<APlayerState>(AbilitySystem->GetOwnerActor()) : nullptr;
		}
		if (const APawn* InstigatorPawn = Actor->GetInstigator())
		{
			return InstigatorPawn->GetPlayerState();
		}
		return nullptr;
	}

	//ダメージ・回復など SetByCaller で数値を 1 つ渡す GE を相手に付ける
	bool ApplySetByCallerEffect(AActor* Instigator, AActor* Target, TSubclassOf<UGameplayEffect> EffectClass, const FGameplayTag& DataTag, float Magnitude, AActor* Causer)
	{
		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
		if (!TargetASC)
		{
			return false;
		}

		//攻撃側が ASC を持たない場合(環境ダメージなど)は相手側の ASC で効果を作る
		UAbilitySystemComponent* SourceASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Instigator);
		UAbilitySystemComponent* SpecOwnerASC = SourceASC ? SourceASC : TargetASC;

		FGameplayEffectContextHandle Context = SpecOwnerASC->MakeEffectContext();
		Context.AddInstigator(Instigator, Causer);

		const FGameplayEffectSpecHandle SpecHandle = SpecOwnerASC->MakeOutgoingSpec(EffectClass, 1.0f, Context);
		if (!SpecHandle.IsValid())
		{
			return false;
		}
		SpecHandle.Data->SetSetByCallerMagnitude(DataTag, Magnitude);

		SpecOwnerASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
		return true;
	}
}

bool USavaAbilitySystemLibrary::ApplyDamage(AActor* DamageInstigator, AActor* Target, float Damage, AActor* DamageCauser)
{
	if (!Target || !Target->HasAuthority() || Damage <= 0.0f)
	{
		return false;
	}

	return ApplySetByCallerEffect(DamageInstigator, Target, USavaAbilitySettings::GetDamageEffectClass(), SavaGameplayTags::SetByCaller_Damage, Damage, DamageCauser);
}

bool USavaAbilitySystemLibrary::ApplyHealing(AActor* HealInstigator, AActor* Target, float Amount)
{
	if (!Target || !Target->HasAuthority() || Amount <= 0.0f)
	{
		return false;
	}
	return ApplySetByCallerEffect(HealInstigator, Target, USavaAbilitySettings::GetHealingEffectClass(), SavaGameplayTags::SetByCaller_Healing, Amount, HealInstigator);
}

FActiveGameplayEffectHandle USavaAbilitySystemLibrary::ApplyEffectToTarget(AActor* EffectInstigator, AActor* Target, TSubclassOf<UGameplayEffect> EffectClass, float Level)
{
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
	if (!TargetASC || !EffectClass || !Target->HasAuthority())
	{
		return FActiveGameplayEffectHandle();
	}

	UAbilitySystemComponent* SourceASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(EffectInstigator);
	UAbilitySystemComponent* SpecOwnerASC = SourceASC ? SourceASC : TargetASC;

	FGameplayEffectContextHandle Context = SpecOwnerASC->MakeEffectContext();
	Context.AddInstigator(EffectInstigator, EffectInstigator);

	const FGameplayEffectSpecHandle SpecHandle = SpecOwnerASC->MakeOutgoingSpec(EffectClass, Level, Context);
	if (!SpecHandle.IsValid())
	{
		return FActiveGameplayEffectHandle();
	}
	return SpecOwnerASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
}

APlayerState* USavaAbilitySystemLibrary::GetOwningPlayerState(const AActor* Actor)
{
	return const_cast<APlayerState*>(FindOwningPlayerState(Actor));
}

uint8 USavaAbilitySystemLibrary::GetTeamId(const AActor* Actor)
{
	const ASavaPlayerState* PlayerState = Cast<ASavaPlayerState>(FindOwningPlayerState(Actor));
	return PlayerState ? PlayerState->GetTeamId() : ASavaPlayerState::NoTeam;
}

bool USavaAbilitySystemLibrary::AreEnemies(const AActor* A, const AActor* B)
{
	if (!A || !B)
	{
		return false;
	}

	//同じプレイヤーの物同士(自分と自分の設置物など)は敵ではない
	const APlayerState* PlayerStateA = FindOwningPlayerState(A);
	const APlayerState* PlayerStateB = FindOwningPlayerState(B);
	if (PlayerStateA && PlayerStateA == PlayerStateB)
	{
		return false;
	}

	const uint8 TeamA = GetTeamId(A);
	const uint8 TeamB = GetTeamId(B);
	if (TeamA == ASavaPlayerState::NoTeam || TeamB == ASavaPlayerState::NoTeam)
	{
		return true;
	}
	return TeamA != TeamB;
}

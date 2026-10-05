// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/SavaAbilityLoadoutComponent.h"
#include "AbilitySystem/SavaAttributeSet.h"
#include "AbilitySystem/SavaGameplayAbility.h"
#include "AbilitySystem/SavaLoadoutAbilityData.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

USavaAbilityLoadoutComponent::USavaAbilityLoadoutComponent()
{
	SetIsReplicatedByDefault(true);
}

USavaAbilityLoadoutComponent* USavaAbilityLoadoutComponent::FindAbilityLoadoutComponent(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<USavaAbilityLoadoutComponent>() : nullptr;
}

void USavaAbilityLoadoutComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(USavaAbilityLoadoutComponent, Skill);
	DOREPLIFETIME(USavaAbilityLoadoutComponent, Gadget);
}

void USavaAbilityLoadoutComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwner()->HasAuthority())
	{
		if (DefaultSkill && !Skill)
		{
			SetSkill(DefaultSkill);
		}
		if (DefaultGadget && !Gadget)
		{
			SetGadget(DefaultGadget);
		}
	}
}

//--------------------------------Loadout

void USavaAbilityLoadoutComponent::SetSkill(USavaSkillData* NewSkill)
{
	if (!GetOwner()->HasAuthority() || Skill == NewSkill)
	{
		return;
	}

	Skill = NewSkill;
	GrantSlot(Skill, SkillHandle);
	OnSkillChanged.Broadcast(Skill);
}

void USavaAbilityLoadoutComponent::SetGadget(USavaGadgetData* NewGadget)
{
	if (!GetOwner()->HasAuthority() || Gadget == NewGadget)
	{
		return;
	}

	Gadget = NewGadget;
	GrantSlot(Gadget, GadgetHandle);

	//ガジェットを外したら残り個数も 0 に戻す(新しいガジェットの個数は、能力が付与されたときに能力側が入れる)
	if (!Gadget)
	{
		if (UAbilitySystemComponent* AbilitySystem = GrantedTo.Get())
		{
			AbilitySystem->SetNumericAttributeBase(USavaAttributeSet::GetMaxGadgetChargesAttribute(), 0.0f);
			AbilitySystem->SetNumericAttributeBase(USavaAttributeSet::GetGadgetChargesAttribute(), 0.0f);
		}
	}
	OnGadgetChanged.Broadcast(Gadget);
}

void USavaAbilityLoadoutComponent::OnRep_Skill()
{
	OnSkillChanged.Broadcast(Skill);
}

void USavaAbilityLoadoutComponent::OnRep_Gadget()
{
	OnGadgetChanged.Broadcast(Gadget);
}

//--------------------------------Grant

void USavaAbilityLoadoutComponent::GrantAbilities(UAbilitySystemComponent* AbilitySystemComponent)
{
	if (!AbilitySystemComponent || !AbilitySystemComponent->IsOwnerActorAuthoritative())
	{
		return;
	}

	//別の ASC に付けていた分は外してから付け直す
	RevokeAbilities();

	GrantedTo = AbilitySystemComponent;
	GrantSlot(Skill, SkillHandle);
	GrantSlot(Gadget, GadgetHandle);
}

void USavaAbilityLoadoutComponent::RevokeAbilities()
{
	ClearSlot(SkillHandle);
	ClearSlot(GadgetHandle);
	GrantedTo.Reset();
}

void USavaAbilityLoadoutComponent::GrantSlot(const USavaLoadoutAbilityData* Data, FGameplayAbilitySpecHandle& InOutHandle)
{
	ClearSlot(InOutHandle);

	UAbilitySystemComponent* AbilitySystem = GrantedTo.Get();
	if (!AbilitySystem || !Data || !Data->Ability)
	{
		return;
	}

	//ボタンのタグを能力に持たせておき、入力時にこのタグで探す(USavaAbilitySet と同じ)
	FGameplayAbilitySpec Spec(Data->Ability->GetDefaultObject<USavaGameplayAbility>(), Data->AbilityLevel);
	if (Data->InputTag.IsValid())
	{
		Spec.GetDynamicSpecSourceTags().AddTag(Data->InputTag);
	}
	InOutHandle = AbilitySystem->GiveAbility(Spec);
}

void USavaAbilityLoadoutComponent::ClearSlot(FGameplayAbilitySpecHandle& InOutHandle)
{
	UAbilitySystemComponent* AbilitySystem = GrantedTo.Get();
	if (AbilitySystem && InOutHandle.IsValid())
	{
		AbilitySystem->ClearAbility(InOutHandle);
	}
	InOutHandle = FGameplayAbilitySpecHandle();
}

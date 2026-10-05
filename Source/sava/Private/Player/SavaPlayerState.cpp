// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/SavaPlayerState.h"
#include "AbilitySystem/SavaAbilitySystemComponent.h"
#include "AbilitySystem/SavaAbilitySystemLibrary.h"
#include "AbilitySystem/SavaAttributeSet.h"
#include "SavaGameplayTags.h"
#include "Net/UnrealNetwork.h"

ASavaPlayerState::ASavaPlayerState()
{
	AbilitySystemComponent = CreateDefaultSubobject<USavaAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	//Mixed: 自分には効果の詳細を、他のプレイヤーには最小限(タグ・Cue)だけ送る
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	//ASC が自動で見つける(この Actor のサブオブジェクトの AttributeSet を登録する)
	AttributeSet = CreateDefaultSubobject<USavaAttributeSet>(TEXT("AttributeSet"));

	//PlayerState の既定の更新頻度は低いため、能力の同期が遅れないように上げる
	SetNetUpdateFrequency(100.0f);
}

UAbilitySystemComponent* ASavaPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ASavaPlayerState::SetTeamId(uint8 NewTeamId)
{
	if (HasAuthority())
	{
		TeamId = NewTeamId;
	}
}

void ASavaPlayerState::Respawn()
{
	if (!HasAuthority() || !AbilitySystemComponent)
	{
		return;
	}

	//先にタグを外す(付いたままだとパッシブ能力が発動できない)
	if (AbilitySystemComponent->HasMatchingGameplayTag(SavaGameplayTags::State_Dead))
	{
		AbilitySystemComponent->RemoveLooseGameplayTag(SavaGameplayTags::State_Dead);
	}

	//HP は回復の GE で戻す。直接書き換えると、HP 0 を通知済みのフラグが戻らず、次の死亡が通知されなくなる
	USavaAbilitySystemLibrary::ApplyHealing(this, this, AttributeSet->GetMaxHealth());
}

void ASavaPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASavaPlayerState, TeamId);
}

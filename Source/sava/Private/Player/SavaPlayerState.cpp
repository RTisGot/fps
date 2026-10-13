// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/SavaPlayerState.h"
#include "AbilitySystem/SavaAbilitySystemComponent.h"
#include "AbilitySystem/SavaAbilitySystemLibrary.h"
#include "AbilitySystem/SavaAttributeSet.h"
#include "AbilitySystem/SavaAbilityLoadoutComponent.h"
#include "AbilitySystem/SavaLoadoutAbilityData.h"
#include "Loadout/SavaLoadoutSettings.h"
#include "Weapon/SavaEquipmentComponent.h"
#include "Weapon/SavaWeaponData.h"
#include "SavaGameplayTags.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogSavaPlayerState, Log, All);

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

void ASavaPlayerState::AddKill()
{
	if (!HasAuthority())
	{
		return;
	}

	++KillCount;

	UE_LOG(LogTemp, Log, TEXT("KillCount increased: PlayerState = %s, KillCount = %d"), *GetName(), KillCount);
}

void ASavaPlayerState::AddDeath()
{
	if (!HasAuthority())
	{
		return;
	}

	++DeathCount;

	UE_LOG(LogTemp, Log, TEXT("DeathCount increased: PlayerState = %s, DeathCount = %d"), *GetName(), DeathCount);
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

void ASavaPlayerState::ResetForNewRound()
{
	if (!HasAuthority() || !AbilitySystemComponent)
	{
		return;
	}

	AbilitySystemComponent->CancelAllAbilities();
	AbilitySystemComponent->RemoveActiveEffects(FGameplayEffectQuery()); //死亡時と同じく、期間付き・永続の GE をすべて外す
	Respawn();
}

//--------------------------------Loadout

void ASavaPlayerState::SetLoadout(const FSavaLoadout& NewLoadout)
{
	if (!HasAuthority())
	{
		return;
	}

	//クライアントから届いた値なので、選べるものだけか必ず確かめる
	Loadout = USavaLoadoutSettings::Sanitize(NewLoadout);
	bHasReceivedLoadout = true;
	UE_LOG(LogSavaPlayerState, Log, TEXT("%s のロードアウト: %s / %s / %s / %s"), *GetPlayerName(),
		*Loadout.PrimaryWeapon.GetAssetName(), *Loadout.SecondaryWeapon.GetAssetName(), *Loadout.Skill.GetAssetName(), *Loadout.Gadget.GetAssetName());

	ApplyLoadoutTo(GetPawn());
	OnLoadoutChanged.Broadcast(Loadout); //サーバーでは OnRep が呼ばれないので自分で呼ぶ
}

void ASavaPlayerState::ApplyLoadoutTo(APawn* TargetPawn) const
{
	if (!HasAuthority() || !TargetPawn)
	{
		return;
	}

	//同じものを入れ直すと弾が満タンに戻ったりするので、変わった枠だけ入れ替える
	if (USavaEquipmentComponent* Equipment = USavaEquipmentComponent::FindEquipmentComponent(TargetPawn))
	{
		USavaWeaponData* Primary = Loadout.PrimaryWeapon.LoadSynchronous();
		if (Primary && Equipment->GetWeaponData(ESavaWeaponSlot::Primary) != Primary)
		{
			Equipment->SetLoadout(ESavaWeaponSlot::Primary, Primary, {});
		}

		USavaWeaponData* Secondary = Loadout.SecondaryWeapon.LoadSynchronous();
		if (Secondary && Equipment->GetWeaponData(ESavaWeaponSlot::Secondary) != Secondary)
		{
			Equipment->SetLoadout(ESavaWeaponSlot::Secondary, Secondary, {});
		}
	}

	if (USavaAbilityLoadoutComponent* AbilityLoadout = USavaAbilityLoadoutComponent::FindAbilityLoadoutComponent(TargetPawn))
	{
		USavaSkillData* Skill = Loadout.Skill.LoadSynchronous();
		if (Skill && AbilityLoadout->GetSkill() != Skill)
		{
			AbilityLoadout->SetSkill(Skill);
		}

		USavaGadgetData* Gadget = Loadout.Gadget.LoadSynchronous();
		if (Gadget && AbilityLoadout->GetGadget() != Gadget)
		{
			AbilityLoadout->SetGadget(Gadget);
		}
	}
}

void ASavaPlayerState::OnRep_Loadout()
{
	OnLoadoutChanged.Broadcast(Loadout);
}

void ASavaPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);

	if (ASavaPlayerState* NewPlayerState = Cast<ASavaPlayerState>(PlayerState))
	{
		NewPlayerState->Loadout = Loadout;
		NewPlayerState->bHasReceivedLoadout = bHasReceivedLoadout;
	}
}

void ASavaPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASavaPlayerState, TeamId);
	DOREPLIFETIME(ASavaPlayerState, KillCount);
	DOREPLIFETIME(ASavaPlayerState, DeathCount);
	DOREPLIFETIME(ASavaPlayerState, Loadout);
}

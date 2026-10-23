// Fill out your copyright notice in the Description page of Project Settings.

#include "Weapon/SavaWeaponReloadAbility.h"
#include "Weapon/SavaEquipmentComponent.h"
#include "Weapon/SavaWeaponData.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "SavaGameplayTags.h"

namespace
{
	//撃つボタン・ダッシュを調べる間隔
	constexpr float LocalTickInterval = 1.0f / 60.0f;
}

USavaWeaponReloadAbility::USavaWeaponReloadAbility()
{
	ActivationPolicy = ESavaAbilityActivationPolicy::OnInputTriggered;

	AddAssetTag(SavaGameplayTags::Ability_Weapon_Reload);
	ActivationOwnedTags.AddTag(SavaGameplayTags::State_Weapon_Reloading);
	CancelAbilitiesWithTag.AddTag(SavaGameplayTags::Ability_Weapon_Fire);
}

USavaWeaponReloadAbility* USavaWeaponReloadAbility::FindActiveReload(const UAbilitySystemComponent* AbilitySystem)
{
	if (!AbilitySystem)
	{
		return nullptr;
	}
	for (const FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
	{
		if (Spec.IsActive() && Spec.Ability && Spec.Ability->IsA<USavaWeaponReloadAbility>())
		{
			return Cast<USavaWeaponReloadAbility>(Spec.GetPrimaryInstance());
		}
	}
	return nullptr;
}

bool USavaWeaponReloadAbility::CanBeInterruptedByFire() const
{
	switch (Phase)
	{
	case EPhase::BeforeInsert:
		return false;
	case EPhase::RoundStart:
	case EPhase::Round:
	{
		const USavaEquipmentComponent* Equipment = GetEquipmentComponent();
		return Equipment && Equipment->GetAmmoInMagazine(ReloadSlot) > 0;
	}
	default:
		return true;
	}
}

bool USavaWeaponReloadAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const USavaEquipmentComponent* Equipment = GetEquipmentFromActorInfo(ActorInfo);
	//武器を出している途中はリロードしない(自分の画面だけで止める。サーバーは持ち替えを知るのが遅れるので、そこで止めると食い違う)
	if (Equipment && ActorInfo->IsLocallyControlled() && Equipment->IsEquipping())
	{
		return false;
	}
	return Equipment && Equipment->CanReload(Equipment->GetCurrentSlot());
}

void USavaWeaponReloadAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	//※ Super::ActivateAbility は呼ばない(Blueprint の Event ActivateAbility は使わない設計)

	USavaEquipmentComponent* Equipment = GetEquipmentFromActorInfo(ActorInfo);
	if (!Equipment || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ReloadSlot = Equipment->GetCurrentSlot();
	const FSavaWeaponStats Stats = Equipment->GetStats(ReloadSlot);
	const USavaWeaponData* Weapon = Equipment->GetWeaponData(ReloadSlot);
	const int32 AmmoAtStart = Equipment->GetAmmoInMagazine(ReloadSlot);

	Phase = EPhase::None;
	bEmptyReload = AmmoAtStart <= 0;
	Capacity = Stats.GetMagazineCapacity(AmmoAtStart > 0);
	bAmmoLoaded = false;
	bFinished = false;
	bFireHeldSinceStart = IsFireInputPressed();
	PlayingMontage = nullptr;

	if (IsLocallyControlled())
	{
		GetWorld()->GetTimerManager().SetTimer(LocalTickTimerHandle, this, &USavaWeaponReloadAbility::OnLocalTick, LocalTickInterval, true);
	}

	if (Stats.ReloadType == ESavaReloadType::Magazine)
	{
		//演出(自分の画面だけ)。アニメーションはリロードの時間で終わる速さで再生する
		const float Duration = Stats.GetReloadDuration(bEmptyReload);
		if (IsLocallyControlled())
		{
			if (Weapon && Weapon->ReloadSound)
			{
				UGameplayStatics::PlaySound2D(this, Weapon->ReloadSound);
			}
			if (Weapon)
			{
				PlayReloadMontage(bEmptyReload && Weapon->EmptyReloadMontage ? Weapon->EmptyReloadMontage : Weapon->ReloadMontage, Duration);
			}
			OnReloadStarted(Duration);
		}
		StartPhase(EPhase::BeforeInsert, Stats.GetReloadInsertTime(bEmptyReload));
	}
	else
	{
		if (IsLocallyControlled())
		{
			//満タンまで込めた場合の時間を見込みとして伝える
			const int32 Reserve = Equipment->HasInfiniteReserveAmmo(ReloadSlot) ? Capacity : Equipment->GetReserveAmmo(ReloadSlot);
			const int32 Rounds = FMath::Min(Capacity - AmmoAtStart, Reserve);
			if (Weapon)
			{
				PlayReloadMontage(Weapon->ReloadStartMontage, Stats.ReloadStartTime);
			}
			OnReloadStarted(Stats.ReloadStartTime + Rounds * Stats.ReloadRoundTime + Stats.ReloadEndTime);
		}
		StartPhase(EPhase::RoundStart, Stats.ReloadStartTime);
	}
}

void USavaWeaponReloadAbility::StartPhase(EPhase NewPhase, float Duration)
{
	Phase = NewPhase;
	PhaseStartTime = GetWorld()->GetTimeSeconds();
	PhaseDuration = Duration;

	if (Duration <= 0.0f)
	{
		OnPhaseElapsed();
		return;
	}
	GetWorld()->GetTimerManager().SetTimer(PhaseTimerHandle, this, &USavaWeaponReloadAbility::OnPhaseElapsed, Duration, false);
}

void USavaWeaponReloadAbility::OnPhaseElapsed()
{
	const USavaEquipmentComponent* Equipment = GetEquipmentComponent();
	if (!Equipment || !IsActive())
	{
		return;
	}
	const FSavaWeaponStats Stats = Equipment->GetStats(ReloadSlot);

	switch (Phase)
	{
	case EPhase::BeforeInsert:
		LoadRounds(Capacity);
		//サーバーは弾を入れたら終わる(残りの動作は見た目だけ)
		if (IsServerOnly())
		{
			Finish();
			return;
		}
		//撃てる状態にしてから、押しておいた撃つボタンで撃つ
		Phase = EPhase::AfterInsert;
		if (TryFireBuffered())
		{
			return;
		}
		StartPhase(EPhase::AfterInsert,Stats.GetReloadDuration(bEmptyReload) - Stats.GetReloadInsertTime(bEmptyReload));
		return;

	case EPhase::Round:
		LoadRounds(1);
		if (TryFireBuffered())
		{
			return;
		}
		StartNextRoundOrEnd();
		return;

	case EPhase::RoundStart:
		StartNextRoundOrEnd();
		return;

	case EPhase::AfterInsert:
	case EPhase::RoundEnd:
	case EPhase::None:
		Finish();
		return;
	}
}

void USavaWeaponReloadAbility::StartNextRoundOrEnd()
{
	const USavaEquipmentComponent* Equipment = GetEquipmentComponent();
	if (!Equipment)
	{
		return;
	}
	const FSavaWeaponStats Stats = Equipment->GetStats(ReloadSlot);
	const USavaWeaponData* Weapon = Equipment->GetWeaponData(ReloadSlot);

	const bool bHasReserve = Equipment->HasInfiniteReserveAmmo(ReloadSlot) || Equipment->GetReserveAmmo(ReloadSlot) > 0;
	if (Equipment->GetAmmoInMagazine(ReloadSlot) < Capacity && bHasReserve)
	{
		if (IsLocallyControlled() && Weapon)
		{
			if (Weapon->ReloadSound)
			{
				UGameplayStatics::PlaySound2D(this, Weapon->ReloadSound);
			}
			PlayReloadMontage(Weapon->ReloadMontage, Stats.ReloadRoundTime);
		}
		StartPhase(EPhase::Round, Stats.ReloadRoundTime);
		return;
	}

	//込め終わった。サーバーは終わりの動作を待たない
	if (IsServerOnly())
	{
		Finish();
		return;
	}
	if (Weapon)
	{
		PlayReloadMontage(Weapon->ReloadEndMontage, Stats.ReloadEndTime);
	}
	StartPhase(EPhase::RoundEnd, Stats.ReloadEndTime);
}

int32 USavaWeaponReloadAbility::LoadRounds(int32 MaxRounds)
{
	USavaEquipmentComponent* Equipment = GetEquipmentComponent();
	const int32 Loaded = Equipment ? Equipment->LoadAmmo(ReloadSlot, MaxRounds, Capacity) : 0;
	if (Loaded > 0)
	{
		bAmmoLoaded = true;
		if (IsLocallyControlled())
		{
			OnReloadAmmoLoaded(Equipment->GetAmmoInMagazine(ReloadSlot));
		}
	}
	return Loaded;
}

bool USavaWeaponReloadAbility::TryFireBuffered()
{
	if (!IsLocallyControlled() || bFireHeldSinceStart)
	{
		return false;
	}

	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo();
	if (!AbilitySystem)
	{
		return false;
	}
	for (const FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
	{
		if (Spec.InputPressed && Spec.Ability && Spec.Ability->GetAssetTags().HasTagExact(SavaGameplayTags::Ability_Weapon_Fire))
		{
			//撃つ能力が、このリロードを止める(Cancel Abilities With Tag)
			AbilitySystem->TryActivateAbility(Spec.Handle);
			return !IsActive();
		}
	}
	return false;
}

void USavaWeaponReloadAbility::OnLocalTick()
{
	if (!IsActive())
	{
		return;
	}

	//一度離したら、次に押したときは弾が入った瞬間に撃つ
	if (!IsFireInputPressed())
	{
		bFireHeldSinceStart = false;
	}

	//弾が入った後の動作は、ダッシュで飛ばす
	const USavaEquipmentComponent* Equipment = GetEquipmentComponent();
	if ((Phase == EPhase::AfterInsert || Phase == EPhase::RoundEnd) && Equipment && Equipment->IsOwnerSprinting())
	{
		CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
	}
}

bool USavaWeaponReloadAbility::IsFireInputPressed() const
{
	const UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo();
	if (!AbilitySystem)
	{
		return false;
	}
	for (const FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.Ability->GetAssetTags().HasTagExact(SavaGameplayTags::Ability_Weapon_Fire))
		{
			return Spec.InputPressed;
		}
	}
	return false;
}

bool USavaWeaponReloadAbility::IsServerOnly() const
{
	return !IsLocallyControlled();
}

void USavaWeaponReloadAbility::Finish()
{
	bFinished = true;

	//終了は伝えない(自分の画面とサーバーがそれぞれ終える。サーバーは弾を入れた時点で終わるので、伝えると自分の画面の動作が途中で切れる)
	//ホスト・一人プレイは、他のプレイヤーへ伝える
	const bool bReplicateEnd = IsLocallyControlled() && CurrentActorInfo && CurrentActorInfo->IsNetAuthority();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, bReplicateEnd, false);
}

void USavaWeaponReloadAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (IsActive())
	{
		if (const UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(PhaseTimerHandle);
			World->GetTimerManager().ClearTimer(LocalTickTimerHandle);
		}

		//サーバー: 自分の画面では弾が入った直後に撃った・持ち替えた場合、サーバーのリロードはまだ少し手前にいる。その分は入れる
		if (!bFinished && IsServerOnly() && (Phase == EPhase::BeforeInsert || Phase == EPhase::Round))
		{
			const double PhaseEndTime = PhaseStartTime + PhaseDuration;
			if (GetWorld()->GetTimeSeconds() >= PhaseEndTime - ServerLoadTolerance)
			{
				LoadRounds(Phase == EPhase::BeforeInsert ? Capacity : 1);
			}
		}

		if (IsLocallyControlled())
		{
			if (!bFinished)
			{
				StopReloadMontage();
			}
			OnReloadEnded(bAmmoLoaded);
		}
		Phase = EPhase::None;
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void USavaWeaponReloadAbility::PlayReloadMontage(UAnimMontage* Montage, float Duration)
{
	const USavaEquipmentComponent* Equipment = GetEquipmentComponent();
	const USkeletalMeshComponent* ArmsMesh = Equipment ? Equipment->GetFirstPersonMesh() : nullptr;
	UAnimInstance* AnimInstance = ArmsMesh ? ArmsMesh->GetAnimInstance() : nullptr;
	if (!AnimInstance || !Montage)
	{
		return;
	}

	const float PlayRate = Duration > 0.0f ? Montage->GetPlayLength() / Duration : 1.0f;
	AnimInstance->Montage_Play(Montage, PlayRate);
	PlayingMontage = Montage;
}

void USavaWeaponReloadAbility::StopReloadMontage()
{
	const USavaEquipmentComponent* Equipment = GetEquipmentComponent();
	const USkeletalMeshComponent* ArmsMesh = Equipment ? Equipment->GetFirstPersonMesh() : nullptr;
	if (UAnimInstance* AnimInstance = ArmsMesh ? ArmsMesh->GetAnimInstance() : nullptr; AnimInstance && PlayingMontage)
	{
		AnimInstance->Montage_Stop(0.2f, PlayingMontage);
	}
	PlayingMontage = nullptr;
}

// Fill out your copyright notice in the Description page of Project Settings.

#include "Weapon/SavaWeaponReloadAbility.h"
#include "Weapon/SavaEquipmentComponent.h"
#include "Weapon/SavaWeaponData.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "SavaGameplayTags.h"

USavaWeaponReloadAbility::USavaWeaponReloadAbility()
{
	ActivationPolicy = ESavaAbilityActivationPolicy::OnInputTriggered;

	AddAssetTag(SavaGameplayTags::Ability_Weapon_Reload);
	ActivationOwnedTags.AddTag(SavaGameplayTags::State_Weapon_Reloading);
	CancelAbilitiesWithTag.AddTag(SavaGameplayTags::Ability_Weapon_Fire);
}

bool USavaWeaponReloadAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const USavaEquipmentComponent* Equipment = GetEquipmentFromActorInfo(ActorInfo);
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
	bReloadCompleted = false;
	const float ReloadTime = Equipment->GetStats(ReloadSlot).ReloadTime;

	//演出(自分の画面だけ)。アニメーションは Reload Time で終わる速さで再生する
	if (IsLocallyControlled())
	{
		if (const USavaWeaponData* Weapon = Equipment->GetWeaponData(ReloadSlot))
		{
			if (Weapon->ReloadSound)
			{
				UGameplayStatics::PlaySound2D(this, Weapon->ReloadSound);
			}
			const USkeletalMeshComponent* ArmsMesh = Equipment->GetFirstPersonMesh();
			if (UAnimInstance* AnimInstance = ArmsMesh ? ArmsMesh->GetAnimInstance() : nullptr; AnimInstance && Weapon->ReloadMontage)
			{
				const float PlayRate = ReloadTime > 0.0f ? Weapon->ReloadMontage->GetPlayLength() / ReloadTime : 1.0f;
				AnimInstance->Montage_Play(Weapon->ReloadMontage, PlayRate);
			}
		}
		OnReloadStarted(ReloadTime);
	}

	if (ReloadTime <= 0.0f)
	{
		OnReloadTimeElapsed();
		return;
	}

	//自分の画面とサーバーがそれぞれ待つ
	UAbilityTask_WaitDelay* WaitTask = UAbilityTask_WaitDelay::WaitDelay(this, ReloadTime);
	WaitTask->OnFinish.AddDynamic(this, &USavaWeaponReloadAbility::OnReloadTimeElapsed);
	WaitTask->ReadyForActivation();
}

void USavaWeaponReloadAbility::OnReloadTimeElapsed()
{
	if (USavaEquipmentComponent* Equipment = GetEquipmentComponent())
	{
		Equipment->FinishReload(ReloadSlot);
	}
	bReloadCompleted = true;

	//自分の画面では終了をサーバーへ伝えない(サーバーは自分で待ち終えてから弾を込めるため。先に終わらせると弾を込められない)
	const bool bReplicateEnd = !IsPredictingClient();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, bReplicateEnd, false);
}

void USavaWeaponReloadAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (IsActive() && IsLocallyControlled())
	{
		if (!bReloadCompleted)
		{
			StopReloadMontage();
		}
		OnReloadEnded(bReloadCompleted);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void USavaWeaponReloadAbility::StopReloadMontage()
{
	const USavaEquipmentComponent* Equipment = GetEquipmentComponent();
	const USavaWeaponData* Weapon = Equipment ? Equipment->GetWeaponData(ReloadSlot) : nullptr;
	const USkeletalMeshComponent* ArmsMesh = Equipment ? Equipment->GetFirstPersonMesh() : nullptr;
	if (UAnimInstance* AnimInstance = ArmsMesh ? ArmsMesh->GetAnimInstance() : nullptr; AnimInstance && Weapon && Weapon->ReloadMontage)
	{
		AnimInstance->Montage_Stop(0.2f, Weapon->ReloadMontage);
	}
}

// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/SavaHoldAimAbility.h"
#include "AbilitySystem/SavaAbilityTask_Tick.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayTag.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "SavaGameplayTags.h"

USavaHoldAimAbility::USavaHoldAimAbility()
{
	ActivationPolicy = ESavaAbilityActivationPolicy::OnInputTriggered;

	CancelOnTagsAdded.AddTag(SavaGameplayTags::State_Dead);
}

void USavaHoldAimAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	//※ Super::ActivateAbility は呼ばない(Blueprint の Event ActivateAbility は使わない設計)

	//死亡などでキャンセル(サーバーとクライアントの両方で待つ)
	for (const FGameplayTag& CancelTag : CancelOnTagsAdded)
	{
		UAbilityTask_WaitGameplayTagAdded* WaitTagTask = UAbilityTask_WaitGameplayTagAdded::WaitGameplayTagAdd(this, CancelTag);
		WaitTagTask->Added.AddDynamic(this, &USavaHoldAimAbility::OnCancelTagAdded);
		WaitTagTask->ReadyForActivation();
	}

	if (IsLocallyControlled())
	{
		//操作している本人: プレビューを出し、離されるのを待つ
		bIsAiming = true;
		OnAimStarted();

		USavaAbilityTask_Tick* TickTask = USavaAbilityTask_Tick::CreateTickTask(this);
		TickTask->OnTick.AddUObject(this, &USavaHoldAimAbility::OnAimTick);
		TickTask->ReadyForActivation();

		//すぐ離した(タップ)場合も確定として扱う
		UAbilityTask_WaitInputRelease* WaitReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
		WaitReleaseTask->OnRelease.AddDynamic(this, &USavaHoldAimAbility::OnInputReleased);
		WaitReleaseTask->ReadyForActivation();
	}
	else if (ActorInfo->IsNetAuthority())
	{
		//サーバー(相手がクライアントの場合): 確定した位置が届くのを待つ
		UAbilitySystemComponent* AbilitySystem = ActorInfo->AbilitySystemComponent.Get();
		const FPredictionKey ActivationKey = ActivationInfo.GetActivationPredictionKey();
		AbilitySystem->AbilityTargetDataSetDelegate(Handle, ActivationKey).AddUObject(this, &USavaHoldAimAbility::OnServerTargetDataReceived);
		bListeningForServerTargetData = true;

		//発動の通知より先に届いていた場合
		AbilitySystem->CallReplicatedTargetDataDelegatesIfSet(Handle, ActivationKey);
	}
}

void USavaHoldAimAbility::OnAimTick(float DeltaTime)
{
	if (!bIsAiming)
	{
		return;
	}

	FTransform AimTransform;
	TArray<FVector> PathPoints;
	const bool bValid = ComputeAim(AimTransform, PathPoints) && IsTargetAllowed(AimTransform);
	OnAimUpdated(AimTransform, bValid, PathPoints);
}

void USavaHoldAimAbility::OnInputReleased(float TimeHeld)
{
	if (!bIsAiming)
	{
		return;
	}

	FTransform TargetTransform;
	TArray<FVector> PathPoints;
	if (!ComputeAim(TargetTransform, PathPoints) || !IsTargetAllowed(TargetTransform))
	{
		//置けない場所で離したらキャンセル(クールダウンは消費しない)
		CancelThisAbility();
		return;
	}

	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo();

	//この後のクールダウン開始とサーバーへの送信を、1 つの予測としてまとめる
	FScopedPredictionWindow ScopedPrediction(AbilitySystem, IsPredictingClient());

	if (!CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{
		CancelThisAbility();
		return;
	}

	if (IsPredictingClient())
	{
		//クライアント: 確定した位置をサーバーへ送る(Spawn はサーバーが行う)
		FGameplayAbilityTargetData_LocationInfo* LocationData = new FGameplayAbilityTargetData_LocationInfo();
		LocationData->TargetLocation.LocationType = EGameplayAbilityTargetingLocationType::LiteralTransform;
		LocationData->TargetLocation.LiteralTransform = TargetTransform;
		const FGameplayAbilityTargetDataHandle DataHandle(LocationData);

		AbilitySystem->ServerSetReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey(),
			DataHandle, FGameplayTag(), AbilitySystem->ScopedPredictionKey);
	}
	else
	{
		//サーバーで操作している本人(リッスンサーバーのホスト・一人プレイ)
		OnConfirmed(TargetTransform);
	}

	FinishAiming(true);
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void USavaHoldAimAbility::OnServerTargetDataReceived(const FGameplayAbilityTargetDataHandle& DataHandle, FGameplayTag ApplicationTag)
{
	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo();
	AbilitySystem->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());

	const FGameplayAbilityTargetData* TargetData = DataHandle.Get(0);
	if (!TargetData || !TargetData->HasEndPoint())
	{
		CancelThisAbility();
		return;
	}

	//クライアントから届いた位置は信用せず、届く距離か・条件を満たすかを確認する
	const FTransform TargetTransform = TargetData->GetEndPointTransform();
	if (!IsTargetValid(TargetTransform) || !IsTargetAllowed(TargetTransform)
		|| !CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{
		CancelThisAbility();
		return;
	}

	OnConfirmed(TargetTransform);
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void USavaHoldAimAbility::OnCancelTagAdded()
{
	CancelThisAbility();
}

void USavaHoldAimAbility::CancelAiming()
{
	if (IsActive() && bIsAiming)
	{
		CancelThisAbility();
	}
}

void USavaHoldAimAbility::CancelThisAbility()
{
	if (IsActive())
	{
		FinishAiming(false);
		CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
	}
}

void USavaHoldAimAbility::FinishAiming(bool bConfirmed)
{
	if (bIsAiming)
	{
		bIsAiming = false;
		OnAimEnded(bConfirmed);
	}
}

void USavaHoldAimAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	//どんな終わり方でもプレビューを消す
	FinishAiming(false);

	if (bListeningForServerTargetData)
	{
		bListeningForServerTargetData = false;
		if (UAbilitySystemComponent* AbilitySystem = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
		{
			AbilitySystem->AbilityTargetDataSetDelegate(Handle, ActivationInfo.GetActivationPredictionKey()).RemoveAll(this);
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

//--------------------------------Aim

bool USavaHoldAimAbility::GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	//サーバーでも、そのプレイヤーの視点(複製された向き)で計算できる
	const APawn* Pawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	const AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	if (!Controller)
	{
		return false;
	}
	Controller->GetPlayerViewPoint(OutLocation, OutRotation);
	return true;
}

bool USavaHoldAimAbility::ComputeAim(FTransform& OutTransform, TArray<FVector>& OutPathPoints) const
{
	OutPathPoints.Reset();

	FVector ViewLocation;
	FRotator ViewRotation;
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar || !GetViewPoint(ViewLocation, ViewRotation))
	{
		return false;
	}

	if (AimMode == ESavaAimMode::ProjectileArc)
	{
		const FVector LaunchLocation = ViewLocation + ViewRotation.RotateVector(LaunchOffset);
		const FVector LaunchDirection = ViewRotation.Vector();

		FPredictProjectilePathParams Params(ProjectileRadius, LaunchLocation, LaunchDirection * LaunchSpeed, MaxPredictionTime, ECC_Visibility, const_cast<AActor*>(Avatar));
		Params.SimFrequency = 20.0f;
		FPredictProjectilePathResult Result;
		UGameplayStatics::PredictProjectilePath(Avatar, Params, Result);

		for (const FPredictProjectilePathPointData& Point : Result.PathData)
		{
			OutPathPoints.Add(Point.Location);
		}

		//投げ物は「投げる位置と向き」で生成する
		OutTransform = FTransform(LaunchDirection.Rotation(), LaunchLocation);
		return true;
	}

	//PointOnSurface: 視線の先の面を探す
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SavaHoldAim), false, Avatar);
	FHitResult Hit;
	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * MaxRange;
	bool bHit = Avatar->GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, TraceEnd, ECC_Visibility, QueryParams);

	//空中を狙っている場合は、届く最大距離の真下の地面にする(崖の下などは探さない)
	if (!bHit)
	{
		constexpr float MaxDropDistance = 500.0f;
		bHit = Avatar->GetWorld()->LineTraceSingleByChannel(Hit, TraceEnd, TraceEnd - FVector(0.0f, 0.0f, MaxDropDistance), ECC_Visibility, QueryParams);
	}

	//置く物は、プレイヤーから見て奥向きにする
	const FRotator FacingRotation(0.0f, ViewRotation.Yaw, 0.0f);
	if (!bHit)
	{
		OutTransform = FTransform(FacingRotation, TraceEnd);
		return false;
	}

	OutTransform = FTransform(FacingRotation, Hit.ImpactPoint);
	const float MinNormalZ = FMath::Cos(FMath::DegreesToRadians(MaxSurfaceAngle));
	return Hit.ImpactNormal.Z >= MinNormalZ - KINDA_SMALL_NUMBER;
}

bool USavaHoldAimAbility::IsTargetValid(const FTransform& TargetTransform) const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return false;
	}

	//投げ物は手元から、設置物は狙える距離(+ 真下を探す分)の範囲内から
	const float AllowedDistance = (AimMode == ESavaAimMode::ProjectileArc ? LaunchOffset.Size() + 100.0f : MaxRange + 500.0f) + ServerDistanceTolerance;
	return FVector::Dist(Avatar->GetActorLocation(), TargetTransform.GetLocation()) <= AllowedDistance;
}

bool USavaHoldAimAbility::IsTargetAllowed_Implementation(const FTransform& TargetTransform) const
{
	return true;
}

//--------------------------------イベントの既定の処理(何もしない。Blueprint か C++ の子クラスで実装する)

void USavaHoldAimAbility::OnAimStarted_Implementation()
{
}

void USavaHoldAimAbility::OnAimUpdated_Implementation(const FTransform& AimTransform, bool bIsValid, const TArray<FVector>& PathPoints)
{
}

void USavaHoldAimAbility::OnAimEnded_Implementation(bool bConfirmed)
{
}

void USavaHoldAimAbility::OnConfirmed_Implementation(const FTransform& TargetTransform)
{
}

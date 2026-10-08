#include "SavaGrappleAbility.h"

#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayTag.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "AbilitySystem/SavaAbilityTask_Tick.h"
#include "AbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/RootMotionSource.h"
#include "NativeGameplayTags.h"
#include "SavaCharacterMovementComponent.h"
#include "SavaGameplayTags.h"
#include "SavaGrappleRootMotionSource.h"
#include "SavaGrappleRope.h"
#include "savaCharacter.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Cooldown_Grapple, "Cooldown.Skill.Grapple");

namespace
{
	//サーバーがアンカーの到着を待つ時間(届かなければ終える)
	constexpr float ServerTargetDataTimeout = 2.0f;

	//サーバーで強制的に終える前の猶予(普段は本人の終了が届いて終わる)
	constexpr float ServerTimeoutGrace = 1.0f;

	//クライアントのアンカーと、サーバーで調べ直した位置のずれの許容
	constexpr float AnchorTolerance = 100.0f;

	//これより近い面には引っ張らない
	constexpr float MinPullDistance = 100.0f;
}

USavaGrappleAbility::USavaGrappleAbility()
{
	//グラップルの初期値
	ActivationPolicy = ESavaAbilityActivationPolicy::OnInputTriggered;
	CooldownDuration = FScalableFloat(2.0f);
	CooldownTags.AddTag(TAG_Cooldown_Grapple);
	RopeClass = ASavaGrappleRope::StaticClass();
}

void USavaGrappleAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	//※ Super::ActivateAbility は呼ばない(Blueprint の Event ActivateAbility は使わない設計)

	//InstancedPerActor なので前回の状態が残っている
	bEnding = false;
	bPulling = false;
	PullSourceID = 0;
	Elapsed = 0.0f;
	StalledTime = 0.0f;

	//死んだら終わる(自分の画面とサーバーの両方)
	UAbilityTask_WaitGameplayTagAdded* DeathTask = UAbilityTask_WaitGameplayTagAdded::WaitGameplayTagAdd(this, SavaGameplayTags::State_Dead);
	DeathTask->Added.AddDynamic(this, &ThisClass::OnDied);
	DeathTask->ReadyForActivation();

	//到着・時間切れの確認(自分の画面とサーバーの両方)
	USavaAbilityTask_Tick* TickTask = USavaAbilityTask_Tick::CreateTickTask(this);
	TickTask->OnTick.AddUObject(this, &ThisClass::TickPull);
	TickTask->ReadyForActivation();

	if (IsLocallyControlled())
	{
		//操作している本人: つかめる面がなければ失敗(クールダウンは消費しない)
		FHitResult Anchor;
		FVector PullTarget;
		if (!FindAnchor(Anchor) || !ComputePullTarget(Anchor.ImpactPoint, Anchor.ImpactNormal, PullTarget))
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}

		//離したら終わる(終了はサーバーへも伝わる)
		UAbilityTask_WaitInputRelease* WaitReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
		WaitReleaseTask->OnRelease.AddDynamic(this, &ThisClass::OnInputReleased);
		WaitReleaseTask->ReadyForActivation();
		if (!IsActive())
		{
			return;
		}

		UAbilitySystemComponent* AbilitySystem = ActorInfo->AbilitySystemComponent.Get();

		//クールダウン開始とサーバーへの送信を、1 つの予測としてまとめる
		FScopedPredictionWindow ScopedPrediction(AbilitySystem, IsPredictingClient());
		if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}

		if (IsPredictingClient())
		{
			//クライアント: アンカーをサーバーへ送り、確認を待たずに自分の画面で先に引っ張る
			const FGameplayAbilityTargetDataHandle DataHandle(new FGameplayAbilityTargetData_SingleTargetHit(Anchor));
			AbilitySystem->ServerSetReplicatedTargetData(Handle, ActivationInfo.GetActivationPredictionKey(),
				DataHandle, FGameplayTag(), AbilitySystem->ScopedPredictionKey);
		}
		StartPull(Anchor);
	}
	else if (ActorInfo->IsNetAuthority())
	{
		//サーバー(操作しているのがクライアント): アンカーが届くのを待つ。終了はクライアントから伝わる
		UAbilitySystemComponent* AbilitySystem = ActorInfo->AbilitySystemComponent.Get();
		const FPredictionKey ActivationKey = ActivationInfo.GetActivationPredictionKey();
		AbilitySystem->AbilityTargetDataSetDelegate(Handle, ActivationKey).AddUObject(this, &ThisClass::OnServerTargetDataReceived);
		bListeningForServerTargetData = true;

		//発動の通知より先に届いていた場合
		AbilitySystem->CallReplicatedTargetDataDelegatesIfSet(Handle, ActivationKey);
	}
}

void USavaGrappleAbility::OnServerTargetDataReceived(const FGameplayAbilityTargetDataHandle& DataHandle, FGameplayTag ApplicationTag)
{
	//消費すると保存されていたデータが消える(DataHandle がそれを指していることがある)ので、先に写す
	const FGameplayAbilityTargetDataHandle DataCopy = DataHandle;
	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo();
	AbilitySystem->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());

	if (bPulling || !IsActive())
	{
		return;
	}

	//クライアントから届いた結果は信用せず、サーバーから見てもつかめるかを確認する
	const FGameplayAbilityTargetData* TargetData = DataCopy.Get(0);
	const FHitResult* ClientHit = TargetData && TargetData->HasHitResult() ? TargetData->GetHitResult() : nullptr;
	if (!ClientHit || !IsAnchorPlausible(*ClientHit) || !CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{
		Finish(true);
		return;
	}

	//サーバーでも同じ引っ張りを付ける(付けないと、クライアントの位置が戻されてしまう)
	StartPull(*ClientHit);
}

//--------------------------------Anchor

bool USavaGrappleAbility::GetAim(FVector& Location, FVector& Direction) const
{
	const AsavaCharacter* Character = GetSavaCharacterFromActorInfo();
	if (!Character || !Character->GetFirstPersonCameraComponent()) return false;
	Location = Character->GetFirstPersonCameraComponent()->GetComponentLocation();
	Direction = Character->GetBaseAimRotation().Vector();
	return !Location.ContainsNaN() && !Direction.ContainsNaN();
}

bool USavaGrappleAbility::IsAnchorAllowed(const FHitResult& Hit) const
{
	const UPrimitiveComponent* Component = Hit.GetComponent();
	// A fixed anchor: moving actors / characters must not leave a rope hanging in mid-air.
	return Hit.bBlockingHit && !Hit.bStartPenetrating && Component
		&& Component->Mobility == EComponentMobility::Static
		&& !Hit.ImpactPoint.ContainsNaN() && !Hit.ImpactNormal.IsNearlyZero();
}

bool USavaGrappleAbility::FindAnchor(FHitResult& Hit) const
{
	FVector View;
	FVector Direction;
	if (!GetAim(View, Direction))
	{
		return false;
	}

	const FVector End = View + Direction * MaxRange;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(SavaGrappleAim), false, GetAvatarActorFromActorInfo());
	return GetWorld()->LineTraceSingleByChannel(Hit, View, End, TraceChannel, Params) && IsAnchorAllowed(Hit);
}

bool USavaGrappleAbility::IsAnchorPlausible(const FHitResult& ClientHit) const
{
	FVector ViewLocation;
	FVector Direction;
	if (ClientHit.ImpactPoint.ContainsNaN() || !GetAim(ViewLocation, Direction))
	{
		return false;
	}

	//届く距離か
	const FVector Anchor = ClientHit.ImpactPoint;
	const FVector ToAnchor = Anchor - ViewLocation;
	if (ToAnchor.Size() > MaxRange + ServerDistanceTolerance)
	{
		return false;
	}

	//サーバーの視点から見えている、固定の面か(壁越しのアンカーを防ぐ)
	FHitResult ServerHit;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(SavaGrappleValidate), false, GetAvatarActorFromActorInfo());
	return GetWorld()->LineTraceSingleByChannel(ServerHit, ViewLocation, Anchor + ToAnchor.GetSafeNormal() * 10.0f, TraceChannel, Params)
		&& IsAnchorAllowed(ServerHit) && FVector::Dist(ServerHit.ImpactPoint, Anchor) <= AnchorTolerance;
}

//--------------------------------Pull

bool USavaGrappleAbility::ComputePullTarget(const FVector& AnchorLocation, const FVector& AnchorNormal, FVector& OutTargetLocation) const
{
	const AsavaCharacter* Character = GetSavaCharacterFromActorInfo();
	const USavaCharacterMovementComponent* Movement = GetSavaMovementFromActorInfo();
	if (!Character || !Movement || Movement->MovementMode == MOVE_None || Movement->IsMantling())
	{
		return false;
	}

	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const UCapsuleComponent* DefaultCapsule = Character->GetClass()->GetDefaultObject<ACharacter>()->GetCapsuleComponent();
	const FVector Normal = AnchorNormal.GetSafeNormal();
	if (!Capsule || !DefaultCapsule || Normal.IsNearlyZero())
	{
		return false;
	}

	//しゃがんでいても立った高さで計算する(引っ張り開始時に立ち上がるため)
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = FMath::Max(Capsule->GetScaledCapsuleHalfHeight(),
		DefaultCapsule->GetUnscaledCapsuleHalfHeight() * Capsule->GetShapeScale());

	//面の向きに合わせて離す距離を変える(床なら半分の高さ、壁なら半径)
	const float Support = Radius + (HalfHeight - Radius) * FMath::Abs(Normal.Z);
	OutTargetLocation = AnchorLocation + Normal * (Support + FMath::Max(SurfaceClearance, 1.0f));

	if (FVector::Dist(Character->GetActorLocation(), OutTargetLocation) < MinPullDistance)
	{
		return false;
	}

	const FCollisionQueryParams Params(SCENE_QUERY_STAT(SavaGrappleDestination), false, Character);
	return !GetWorld()->OverlapBlockingTestByProfile(OutTargetLocation, FQuat::Identity, Capsule->GetCollisionProfileName(),
		FCollisionShape::MakeCapsule(Radius, HalfHeight), Params);
}

void USavaGrappleAbility::StartPull(const FHitResult& Anchor)
{
	if (bPulling || bEnding)
	{
		return;
	}

	AsavaCharacter* Character = GetSavaCharacterFromActorInfo();
	USavaCharacterMovementComponent* Movement = GetSavaMovementFromActorInfo();
	if (!Character || !Movement || !ComputePullTarget(Anchor.ImpactPoint, Anchor.ImpactNormal, TargetLocation))
	{
		Finish(true);
		return;
	}

	bPulling = true;
	Elapsed = 0.0f;
	StalledTime = 0.0f;
	LastLocation = Character->GetActorLocation();
	Character->UnCrouch();
	Movement->SetMovementMode(MOVE_Flying);

	TSharedPtr<FSavaGrappleRootMotionSource> Pull = MakeShared<FSavaGrappleRootMotionSource>();
	Pull->InstanceName = TEXT("GrapplePull");
	Pull->Priority = 1000;
	Pull->AccumulateMode = ERootMotionAccumulateMode::Override;
	Pull->StartLocation = Character->GetActorLocation();
	Pull->TargetLocation = TargetLocation;
	Pull->Duration = FMath::Max(MaxPullDuration, 0.1f);
	Pull->PullSpeed = FMath::Max(PullSpeed, 100.0f);
	Pull->SteeringSpeed = FMath::Max(SteeringSpeed, 0.0f);
	Pull->MaxLateralOffset = FMath::Max(MaxLateralOffset, 0.0f);
	Pull->SteeringResponse = FMath::Max(SteeringResponse, 0.1f);
	Pull->JumpLiftSpeed = FMath::Max(JumpLiftSpeed, 0.0f);
	Pull->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::ClampVelocity;
	Pull->FinishVelocityParams.ClampVelocity = 1200.0f;
	PullSourceID = Movement->ApplyRootMotionSource(Pull);

	//ロープは両方で出す。サーバーの物は本人の画面では隠れる(ASavaGrappleRope::Tick)
	if (RopeClass)
	{
		FActorSpawnParameters Spawn;
		Spawn.Owner = Character;
		Spawn.Instigator = Character;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Rope = GetWorld()->SpawnActor<ASavaGrappleRope>(RopeClass, Character->GetActorTransform(), Spawn);
		if (Rope)
		{
			Rope->InitializeAnchor(Anchor.ImpactPoint);
		}
	}

	OnGrappleStarted(Anchor.ImpactPoint);
}

void USavaGrappleAbility::TickPull(float DeltaTime)
{
	Elapsed += DeltaTime;

	//サーバー(相手がクライアント)の時間切れは、本人の終了が届くのを少し待ってから
	const bool bLocal = IsLocallyControlled();

	if (!bPulling)
	{
		//サーバーがアンカーを待っている間(本人の画面では発動と同時に引っ張っている)
		if (Elapsed > ServerTargetDataTimeout)
		{
			Finish(true);
		}
		return;
	}

	const AsavaCharacter* Character = GetSavaCharacterFromActorInfo();
	const USavaCharacterMovementComponent* Movement = GetSavaMovementFromActorInfo();
	if (!Character || !Movement || Movement->MovementMode != MOVE_Flying)
	{
		Finish(true);
		return;
	}

	const FVector Location = Character->GetActorLocation();
	if (FVector::DistSquared(Location, TargetLocation) < FMath::Square(35.0f))
	{
		Finish(false);
		return;
	}

	//止まった判定は本人だけ(サーバーは通信が途切れると動きが止まって見えるため)
	if (bLocal)
	{
		StalledTime = FVector::DistSquared(Location, LastLocation) < 1.0f ? StalledTime + DeltaTime : 0.0f;
		LastLocation = Location;
		if (StalledTime > 0.25f)
		{
			Finish(true);
			return;
		}
	}

	if (Elapsed > FMath::Max(MaxPullDuration, 0.1f) + (bLocal ? 0.0f : ServerTimeoutGrace))
	{
		Finish(true);
	}
}

void USavaGrappleAbility::OnInputReleased(float TimeHeld) { Finish(false); }
void USavaGrappleAbility::OnDied() { Finish(true); }

void USavaGrappleAbility::Finish(bool bCancelled)
{
	if (IsActive() && !bEnding)
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bCancelled);
}

void USavaGrappleAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bEnding || !IsActive())
	{
		return;
	}
	bEnding = true;

	if (bListeningForServerTargetData)
	{
		bListeningForServerTargetData = false;
		if (UAbilitySystemComponent* AbilitySystem = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
		{
			AbilitySystem->AbilityTargetDataSetDelegate(Handle, ActivationInfo.GetActivationPredictionKey()).RemoveAll(this);
		}
	}

	if (Rope)
	{
		Rope->Destroy();
		Rope = nullptr;
	}

	const bool bWasPulling = bPulling;
	bPulling = false;
	USavaCharacterMovementComponent* Movement = GetSavaMovementFromActorInfo();
	if (Movement && PullSourceID != 0)
	{
		Movement->RemoveRootMotionSourceByID(PullSourceID);
		PullSourceID = 0;
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

	if (bWasPulling && Movement && Movement->MovementMode == MOVE_Flying)
	{
		Movement->Velocity = Movement->Velocity.GetClampedToMaxSize(1200.0f);
		Movement->SetMovementMode(MOVE_Falling);
	}
	if (bWasPulling)
	{
		OnGrappleFinished(bWasCancelled);
	}
}

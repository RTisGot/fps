#include "SavaGrappleAbility.h"

#include "Abilities/GameplayAbilityTargetTypes.h"
#include "SavaGrappleRootMotionSource.h"
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
#include "SavaGrappleRope.h"
#include "savaCharacter.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Cooldown_Grapple, "Cooldown.Skill.Grapple");

USavaGrappleAbility::USavaGrappleAbility()
{
	// WaitInputRelease transports release to the server; do not end in InputReleased first.
	ActivationPolicy = ESavaAbilityActivationPolicy::OnInputTriggered;
	CooldownDuration = FScalableFloat(2.0f);
	CooldownTags.AddTag(TAG_Cooldown_Grapple);
	RopeClass = ASavaGrappleRope::StaticClass();
}

void USavaGrappleAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// Native lifecycle; BP children customize defaults and the presentation events below.
	bEnding = false;
	PullSourceID = 0;
	bPulling = false;
	bWaitingForTarget = false;
	Elapsed = StalledTime = 0.0f;
	const USavaCharacterMovementComponent* Movement = GetSavaMovementFromActorInfo();
	if (!ActorInfo || !Movement || Movement->MovementMode == MOVE_None || Movement->IsMantling())
	{
		Finish(true);
		return;
	}

	auto* DeathTask = UAbilityTask_WaitGameplayTagAdded::WaitGameplayTagAdd(this, SavaGameplayTags::State_Dead);
	DeathTask->Added.AddDynamic(this, &ThisClass::OnDied);
	DeathTask->ReadyForActivation();
	if (!IsActive()) return;

	auto* TickTask = USavaAbilityTask_Tick::CreateTickTask(this);
	TickTask->OnTick.AddUObject(this, &ThisClass::TickPull);
	TickTask->ReadyForActivation();

	if (IsLocallyControlled() || !ActorInfo->IsNetAuthority())
	{
		FHitResult Hit;
		if (!FindAnchor(Hit))
		{
			Finish(true);
			return;
		}
		if (IsPredictingClient())
		{
			UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
			FScopedPredictionWindow Prediction(ASC, true);
			const FGameplayAbilityTargetDataHandle Data(new FGameplayAbilityTargetData_SingleTargetHit(Hit));
			ASC->ServerSetReplicatedTargetData(Handle, ActivationInfo.GetActivationPredictionKey(), Data,
				FGameplayTag(), ASC->ScopedPredictionKey);
			StartPull(Hit);
		}
		else
		{
			StartPull(Hit);
		}
	}
	else if (GetSavaCharacterFromActorInfo()->GetNetConnection() == nullptr)
	{
		// Standalone / server-controlled character, with no remote target data to wait for.
		FHitResult Hit;
		if (FindAnchor(Hit)) StartPull(Hit);
		else Finish(true);
	}
	else
	{
		bWaitingForTarget = true;
		UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
		ASC->AbilityTargetDataSetDelegate(Handle, ActivationInfo.GetActivationPredictionKey())
			.AddUObject(this, &ThisClass::ReceiveTargetData);
		ASC->CallReplicatedTargetDataDelegatesIfSet(Handle, ActivationInfo.GetActivationPredictionKey());
	}

	if (!IsActive()) return;
	auto* ReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
	ReleaseTask->OnRelease.AddDynamic(this, &ThisClass::OnReleased);
	ReleaseTask->ReadyForActivation();
}

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
	FVector View, Direction;
	if (!GetAim(View, Direction)) return false;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SavaGrappleAim), false, GetAvatarActorFromActorInfo());
	return GetWorld()->LineTraceSingleByChannel(Hit, View, View + Direction * FMath::Max(MaxRange, 100.0f), TraceChannel, Params)
		&& IsAnchorAllowed(Hit);
}

void USavaGrappleAbility::ReceiveTargetData(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag)
{
	if (!bWaitingForTarget || bPulling || !IsActive()) return;
	// Copy BEFORE consuming: ConsumeClientReplicatedTargetData invalidates its stored handle.
	const FGameplayAbilityTargetDataHandle DataCopy = Data;
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	ASC->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());
	const FGameplayAbilityTargetData* Entry = DataCopy.Get(0);
	const FHitResult* ClientHit = Entry ? Entry->GetHitResult() : nullptr;
	FVector View, Direction;
	if (!ClientHit || ClientHit->ImpactPoint.ContainsNaN() || !GetAim(View, Direction))
	{
		Finish(true);
		return;
	}
	const FVector ToAnchor = ClientHit->ImpactPoint - View;
	// Validate distance, view cone and line of sight on the server. Never trust a client hit.
	if (ToAnchor.Size() > FMath::Max(MaxRange, 100.0f) + 100.0f || ToAnchor.Size() < 50.0f
		|| FVector::DotProduct(ToAnchor.GetSafeNormal(), Direction) < FMath::Cos(FMath::DegreesToRadians(20.0f)))
	{
		Finish(true);
		return;
	}
	FHitResult ServerHit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SavaGrappleValidate), false, GetAvatarActorFromActorInfo());
	if (!GetWorld()->LineTraceSingleByChannel(ServerHit, View, ClientHit->ImpactPoint + ToAnchor.GetSafeNormal() * 10.0f, TraceChannel, Params)
		|| !IsAnchorAllowed(ServerHit) || FVector::Dist(ServerHit.ImpactPoint, ClientHit->ImpactPoint) > 100.0f)
	{
		Finish(true);
		return;
	}
	FScopedPredictionWindow Prediction(ASC, CurrentActivationInfo.GetActivationPredictionKey());
	StartPull(ServerHit);
}

void USavaGrappleAbility::StartPull(const FHitResult& Hit)
{
	AsavaCharacter* Character = GetSavaCharacterFromActorInfo();
	USavaCharacterMovementComponent* Movement = GetSavaMovementFromActorInfo();
	if (!Character || !Movement || Movement->MovementMode == MOVE_None || Movement->IsMantling())
	{
		Finish(true);
		return;
	}
	// Use the standing capsule's support distance so crouch release cannot put us inside a wall.
	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const UCapsuleComponent* DefaultCapsule = Character->GetClass()->GetDefaultObject<ACharacter>()->GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = FMath::Max(Capsule->GetScaledCapsuleHalfHeight(),
		DefaultCapsule->GetUnscaledCapsuleHalfHeight() * Capsule->GetShapeScale());
	const FVector Normal = Hit.ImpactNormal.GetSafeNormal();
	const float Support = Radius + (HalfHeight - Radius) * FMath::Abs(Normal.Z);
	TargetLocation = Hit.ImpactPoint + Normal * (Support + FMath::Max(SurfaceClearance, 1.0f));
	const float Distance = FVector::Dist(Character->GetActorLocation(), TargetLocation);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SavaGrappleDestination), false, Character);
	if (Distance < 100.0f || GetWorld()->OverlapBlockingTestByProfile(TargetLocation, FQuat::Identity,
		Capsule->GetCollisionProfileName(), FCollisionShape::MakeCapsule(Radius, HalfHeight), Params)
		|| !CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{
		Finish(true);
		return;
	}

	bPulling = true;
	Elapsed = StalledTime = 0.0f;
	LastLocation = Character->GetActorLocation();
	Character->UnCrouch();
	// No direct teleport/SetActorLocation. CharacterMovement retains collision and root-motion prediction.
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

	if (RopeClass)
	{
		// The owner predicts a local rope; server replication provides it to the other players.
		FActorSpawnParameters Spawn;
		Spawn.Owner = Character;
		Spawn.Instigator = Character;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Rope = GetWorld()->SpawnActor<ASavaGrappleRope>(RopeClass, Character->GetActorTransform(), Spawn);
		if (Rope) Rope->InitializeAnchor(Hit.ImpactPoint);
	}
	OnGrappleStarted(Hit.ImpactPoint);
}

void USavaGrappleAbility::TickPull(float DeltaTime)
{
	Elapsed += DeltaTime;
	if (!bPulling)
	{
		if (Elapsed > 2.0f) Finish(true); // Missing target data must not leave a live ability behind.
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
	// Stop when a capsule gets stuck on an obstacle instead of pushing into it forever.
	StalledTime = FVector::DistSquared(Location, LastLocation) < 1.0f ? StalledTime + DeltaTime : 0.0f;
	LastLocation = Location;
	if (StalledTime > 0.25f || Elapsed > FMath::Max(MaxPullDuration, 0.1f)) Finish(true);
}

void USavaGrappleAbility::InputReleased(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	// WaitInputRelease handles this after ASC broadcasts its replicated input event.
}

void USavaGrappleAbility::OnReleased(float TimeHeld) { Finish(false); }
void USavaGrappleAbility::OnDied() { Finish(true); }

void USavaGrappleAbility::Finish(bool bCancelled)
{
	if (IsActive() && !bEnding)
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bCancelled);
}

void USavaGrappleAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bEnding || !IsActive()) return;
	bEnding = true;
	if (bWaitingForTarget)
	{
		if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
		{
			ASC->AbilityTargetDataSetDelegate(Handle, ActivationInfo.GetActivationPredictionKey()).RemoveAll(this);
			ASC->ConsumeClientReplicatedTargetData(Handle, ActivationInfo.GetActivationPredictionKey());
		}
		bWaitingForTarget = false;
	}
	if (Rope) { Rope->Destroy(); Rope = nullptr; }
	const bool bWasPulling = bPulling;
	bPulling = false;
	USavaCharacterMovementComponent* Movement = GetSavaMovementFromActorInfo();
	if (Movement && PullSourceID != 0)
	{
		Movement->RemoveRootMotionSourceByID(PullSourceID);
		PullSourceID = 0;
	}
	// End tasks and the steering source before restoring normal physics.
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	if (bWasPulling && Movement && Movement->MovementMode == MOVE_Flying)
	{
		Movement->Velocity = Movement->Velocity.GetClampedToMaxSize(1200.0f);
		Movement->SetMovementMode(MOVE_Falling);
	}
	if (bWasPulling) OnGrappleFinished(bWasCancelled);
}

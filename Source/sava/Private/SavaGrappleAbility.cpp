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
	UE_LOG(LogTemp, Warning, TEXT("USavaGrappleAbility::USavaGrappleAbility() called"));
	//グラップルの初期値
	ActivationPolicy = ESavaAbilityActivationPolicy::OnInputTriggered;
	CooldownDuration = FScalableFloat(2.0f);
	CooldownTags.AddTag(TAG_Cooldown_Grapple);
	RopeClass = ASavaGrappleRope::StaticClass();
	bEndAbilityAfterConfirm = false;
}

/*void USavaGrappleAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
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
}*/

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
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[GRAPPLE TRACE] GetAim FAILED")
		);
		return false;
	}
	const FVector End =
		View +
		Direction * FMath::Max(MaxRange, 100.0f);
	UE_LOG(
		LogTemp,
		Error,
		TEXT("[GRAPPLE TRACE] Start=%s Dir=%s End=%s Range=%.1f"),
		*View.ToString(),
		*Direction.ToString(),
		*End.ToString(),
		MaxRange
	);
	
	FCollisionQueryParams Params(
		SCENE_QUERY_STAT(SavaGrappleAim),
		false,
		GetAvatarActorFromActorInfo()
	);
	const bool bHit =
		GetWorld()->LineTraceSingleByChannel(
			Hit,
			View,
			End,
			TraceChannel,
			Params
		);
	UE_LOG(
		LogTemp,
		Error,
		TEXT("[GRAPPLE TRACE] Hit=%d"),
		bHit ? 1 : 0
	);
	if (!bHit)
	{
		return false;
	}
	UE_LOG(
		LogTemp,
		Error,
		TEXT("[GRAPPLE TRACE] HIT Actor=%s Component=%s Point=%s Normal=%s Mobility=%d"),
		Hit.GetActor()
		? *Hit.GetActor()->GetName()
		: TEXT("NULL"),
		Hit.GetComponent()
		? *Hit.GetComponent()->GetName()
		: TEXT("NULL"),
		*Hit.ImpactPoint.ToString(),
		*Hit.ImpactNormal.ToString(),
		Hit.GetComponent()
		? static_cast<int32>(Hit.GetComponent()->Mobility)
		: -1
	);
	const bool bAllowed =
		IsAnchorAllowed(Hit);
	UE_LOG(
		LogTemp,
		Error,
		TEXT("[GRAPPLE TRACE] AnchorAllowed=%d"),
		bAllowed ? 1 : 0
	);
	return bAllowed;
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
	UE_LOG(
		LogTemp,
		Error,
		TEXT("[GRAPPLE][11] StartPull ENTER Active=%d"),
		IsActive() ? 1 : 0
	);
	AsavaCharacter* Character =
		GetSavaCharacterFromActorInfo();
	USavaCharacterMovementComponent* Movement =
		GetSavaMovementFromActorInfo();
	UE_LOG(
		LogTemp,
		Error,
		TEXT("[GRAPPLE][12] Character=%d Movement=%d Mode=%d Mantling=%d"),
		Character ? 1 : 0,
		Movement ? 1 : 0,
		Movement ? static_cast<int32>(Movement->MovementMode) : -1,
		Movement ? Movement->IsMantling() : 0
	);
	if (!Character ||
		!Movement ||
		Movement->MovementMode == MOVE_None ||
		Movement->IsMantling())
	{
		UE_LOG(LogTemp, Error, TEXT("[GRAPPLE][FAIL-06] Movement check"));
		Finish(true);
		return;
	}
	const UCapsuleComponent* Capsule =
		Character->GetCapsuleComponent();
	const UCapsuleComponent* DefaultCapsule =
		Character->GetClass()
		->GetDefaultObject<ACharacter>()
		->GetCapsuleComponent();
	if (!Capsule || !DefaultCapsule)
	{
		UE_LOG(LogTemp, Error, TEXT("[GRAPPLE][FAIL-07] Capsule NULL"));
		Finish(true);
		return;
	}
	const float Radius =
		Capsule->GetScaledCapsuleRadius();
	const float HalfHeight =
		FMath::Max(
			Capsule->GetScaledCapsuleHalfHeight(),
			DefaultCapsule->GetUnscaledCapsuleHalfHeight()
			* Capsule->GetShapeScale()
		);
	const FVector Normal =
		Hit.ImpactNormal.GetSafeNormal();
	UE_LOG(
		LogTemp,
		Error,
		TEXT("[GRAPPLE][13] Impact=%s Normal=%s Radius=%.2f HalfHeight=%.2f"),
		*Hit.ImpactPoint.ToString(),
		*Normal.ToString(),
		Radius,
		HalfHeight
	);
	if (Normal.IsNearlyZero())
	{
		UE_LOG(LogTemp, Error, TEXT("[GRAPPLE][FAIL-08] Normal ZERO"));
		Finish(true);
		return;
	}
	const float Support =
		Radius +
		(HalfHeight - Radius) *
		FMath::Abs(Normal.Z);
	TargetLocation =
		Hit.ImpactPoint +
		Normal *
		(Support + FMath::Max(SurfaceClearance, 1.0f));
	const float Distance =
		FVector::Dist(
			Character->GetActorLocation(),
			TargetLocation
		);
	UE_LOG(
		LogTemp,
		Error,
		TEXT("[GRAPPLE][14] Player=%s Target=%s Distance=%.2f"),
		*Character->GetActorLocation().ToString(),
		*TargetLocation.ToString(),
		Distance
	);
	if (Distance < 100.0f)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[GRAPPLE][FAIL-09] Distance too short %.2f"),
			Distance
		);
		Finish(true);
		return;
	}
	FCollisionQueryParams Params(
		SCENE_QUERY_STAT(SavaGrappleDestination),
		false,
		Character
	);
	const bool bBlocked =
		GetWorld()->OverlapBlockingTestByProfile(
			TargetLocation,
			FQuat::Identity,
			Capsule->GetCollisionProfileName(),
			FCollisionShape::MakeCapsule(
				Radius,
				HalfHeight
			),
			Params
		);
	UE_LOG(
		LogTemp,
		Error,
		TEXT("[GRAPPLE][15] DestinationBlocked=%d"),
		bBlocked ? 1 : 0
	);
	if (bBlocked)
	{
		UE_LOG(LogTemp, Error, TEXT("[GRAPPLE][FAIL-10] Destination BLOCKED"));
		Finish(true);
		return;
	}
	// HoldAim側ですでにCommitされているので
	// ここではCommitAbility()を呼ばない。
	bPulling = true;
	UE_LOG(
		LogTemp,
		Error,
		TEXT("[GRAPPLE][16] PULLING TRUE")
	);
	Elapsed = 0.0f;
	StalledTime = 0.0f;
	LastLocation =
		Character->GetActorLocation();
	Character->UnCrouch();
	UE_LOG(LogTemp, Error, TEXT("[GRAPPLE][17] Setting MOVE_Flying"));
	Movement->SetMovementMode(MOVE_Flying);
	UE_LOG(
		LogTemp,
		Error,
		TEXT("[GRAPPLE][18] MovementMode=%d"),
		static_cast<int32>(Movement->MovementMode)
	);
	TSharedPtr<FSavaGrappleRootMotionSource> Pull =
		MakeShared<FSavaGrappleRootMotionSource>();
	Pull->InstanceName = TEXT("GrapplePull");
	Pull->Priority = 1000;
	Pull->AccumulateMode =
		ERootMotionAccumulateMode::Override;
	Pull->StartLocation =
		Character->GetActorLocation();
	Pull->TargetLocation =
		TargetLocation;
	Pull->Duration =
		FMath::Max(MaxPullDuration, 0.1f);
	Pull->PullSpeed =
		FMath::Max(PullSpeed, 100.0f);
	Pull->SteeringSpeed =
		FMath::Max(SteeringSpeed, 0.0f);
	Pull->MaxLateralOffset =
		FMath::Max(MaxLateralOffset, 0.0f);
	Pull->SteeringResponse =
		FMath::Max(SteeringResponse, 0.1f);
	Pull->JumpLiftSpeed =
		FMath::Max(JumpLiftSpeed, 0.0f);
	Pull->FinishVelocityParams.Mode =
		ERootMotionFinishVelocityMode::ClampVelocity;
	Pull->FinishVelocityParams.ClampVelocity =
		1200.0f;
	UE_LOG(
		LogTemp,
		Error,
		TEXT("[GRAPPLE][19] Applying RootMotion Speed=%.2f Duration=%.2f"),
		Pull->PullSpeed,
		Pull->Duration
	);
	PullSourceID =
		Movement->ApplyRootMotionSource(Pull);
	UE_LOG(
		LogTemp,
		Error,
		TEXT("[GRAPPLE][20] RootMotion ID=%u"),
		PullSourceID
	);
	UE_LOG(
		LogTemp,
		Error,
		TEXT("[GRAPPLE][21] RopeClass=%s"),
		RopeClass ? *RopeClass->GetName() : TEXT("NULL")
	);
	if (RopeClass)
	{
		FActorSpawnParameters Spawn;
		Spawn.Owner = Character;
		Spawn.Instigator = Character;
		Spawn.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Rope =
			GetWorld()->SpawnActor<ASavaGrappleRope>(
				RopeClass,
				Character->GetActorTransform(),
				Spawn
			);
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[GRAPPLE][22] Rope Spawn=%s"),
			Rope ? TEXT("SUCCESS") : TEXT("FAILED")
		);
		if (Rope)
		{
			Rope->InitializeAnchor(
				Hit.ImpactPoint
			);
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[GRAPPLE][23] Rope Initialized")
			);
		}
	}
	UE_LOG(
		LogTemp,
		Error,
		TEXT("[GRAPPLE][24] Calling OnGrappleStarted")
	);
	OnGrappleStarted(
		Hit.ImpactPoint
	);
	UE_LOG(
		LogTemp,
		Error,
		TEXT("[GRAPPLE][25] StartPull COMPLETE")
	);
}

void USavaGrappleAbility::TickPull(float DeltaTime)
{
	Elapsed += DeltaTime;
	if (!bPulling)
	{
		if (Elapsed > 2.0f)
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[GRAPPLE][FAIL-11] Tick timeout while not pulling")
			);
			Finish(true);
		}
		return;
	}
	const AsavaCharacter* Character =
		GetSavaCharacterFromActorInfo();
	const USavaCharacterMovementComponent* Movement =
		GetSavaMovementFromActorInfo();
	if (!Character || !Movement)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[GRAPPLE][FAIL-12] Tick Character/Movement NULL")
		);
		Finish(true);
		return;
	}
	if (Movement->MovementMode != MOVE_Flying)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[GRAPPLE][FAIL-13] Not Flying Mode=%d"),
			static_cast<int32>(Movement->MovementMode)
		);
		Finish(true);
		return;
	}
	const FVector Location =
		Character->GetActorLocation();
	if (FVector::DistSquared(
		Location,
		TargetLocation) < FMath::Square(35.0f))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[GRAPPLE][26] Reached target")
		);
		Finish(false);
		return;
	}
	StalledTime =
		FVector::DistSquared(
			Location,
			LastLocation) < 1.0f
		? StalledTime + DeltaTime
		: 0.0f;
	LastLocation = Location;
	if (StalledTime > 0.25f)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[GRAPPLE][FAIL-14] STALLED %.3f"),
			StalledTime
		);
		Finish(true);
		return;
	}
	if (Elapsed >
		FMath::Max(MaxPullDuration, 0.1f))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[GRAPPLE][FAIL-15] Duration timeout %.3f"),
			Elapsed
		);
		Finish(true);
	}
}

/*void USavaGrappleAbility::InputReleased(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	// WaitInputRelease handles this after ASC broadcasts its replicated input event.
}*/

void USavaGrappleAbility::OnReleased(float TimeHeld) { Finish(false); }
void USavaGrappleAbility::OnDied() { Finish(true); }

void USavaGrappleAbility::Finish(bool bCancelled)
{
	if (IsActive() && !bEnding)
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bCancelled);
}

void USavaGrappleAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	UE_LOG(
		LogTemp,
		Error,
		TEXT("[GRAPPLE][END] Cancelled=%d Pulling=%d Active=%d Ending=%d RootMotion=%u Rope=%d"),
		bWasCancelled ? 1 : 0,
		bPulling ? 1 : 0,
		IsActive() ? 1 : 0,
		bEnding ? 1 : 0,
		PullSourceID,
		Rope ? 1 : 0
	);
	if (bEnding || !IsActive())
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[GRAPPLE][END] Early return")
		);
		return;
	}
	bEnding = true;
	if (bWaitingForTarget)
	{
		if (UAbilitySystemComponent* ASC =
			ActorInfo
			? ActorInfo->AbilitySystemComponent.Get()
			: nullptr)
		{
			ASC->AbilityTargetDataSetDelegate(
				Handle,
				ActivationInfo.GetActivationPredictionKey()
			).RemoveAll(this);
			ASC->ConsumeClientReplicatedTargetData(
				Handle,
				ActivationInfo.GetActivationPredictionKey()
			);
		}
		bWaitingForTarget = false;
	}
	if (Rope)
	{
		UE_LOG(LogTemp, Error, TEXT("[GRAPPLE][END] Destroying Rope"));
		Rope->Destroy();
		Rope = nullptr;
	}
	const bool bWasPulling = bPulling;
	bPulling = false;
	USavaCharacterMovementComponent* Movement =
		GetSavaMovementFromActorInfo();
	if (Movement && PullSourceID != 0)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[GRAPPLE][END] Removing RootMotion %u"),
			PullSourceID
		);
		Movement->RemoveRootMotionSourceByID(
			PullSourceID
		);
		PullSourceID = 0;
	}
	Super::EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		bReplicateEndAbility,
		bWasCancelled
	);
	if (bWasPulling &&
		Movement &&
		Movement->MovementMode == MOVE_Flying)
	{
		Movement->Velocity =
			Movement->Velocity.GetClampedToMaxSize(
				1200.0f
			);
		Movement->SetMovementMode(
			MOVE_Falling
		);
	}
	if (bWasPulling)
	{
		OnGrappleFinished(
			bWasCancelled
		);
	}
	UE_LOG(LogTemp, Error, TEXT("[GRAPPLE][END] COMPLETE"));
}

bool USavaGrappleAbility::ComputeAim(
	FTransform& OutTransform,
	TArray<FVector>& OutPathPoints) const
{
	UE_LOG(LogTemp, Error, TEXT("[GRAPPLE][05] ComputeAim"));
	return ComputeGrappleAim(
		OutTransform,
		OutPathPoints
	);
	
}
void USavaGrappleAbility::OnConfirmed_Implementation(
	const FTransform& TargetTransform)
{
	UE_LOG(
		LogTemp,
		Error,
		TEXT("[GRAPPLE][08] CONFIRMED Location=%s Normal=%s Active=%d"),
		*TargetTransform.GetLocation().ToString(),
		*TargetTransform.GetRotation().GetUpVector().ToString(),
		IsActive() ? 1 : 0
	);
	FHitResult Hit;
	Hit.bBlockingHit = true;
	Hit.ImpactPoint =
		TargetTransform.GetLocation();
	Hit.Location =
		TargetTransform.GetLocation();
	Hit.ImpactNormal =
		TargetTransform
		.GetRotation()
		.GetUpVector()
		.GetSafeNormal();
	Hit.Normal = Hit.ImpactNormal;
	UE_LOG(LogTemp, Error, TEXT("[GRAPPLE][09] Calling StartPull"));
	
	UE_LOG(
		LogTemp,
		Error,
		TEXT("[GRAPPLE][10] StartPull returned Pulling=%d Active=%d"),
		bPulling ? 1 : 0,
		IsActive() ? 1 : 0
	);
}
void USavaGrappleAbility::OnAimUpdated_Implementation(

	const FTransform& AimTransform,

	bool bIsValid,

	const TArray<FVector>& PathPoints)

{

	if (!bIsValid)

	{

		return;

	}

	// 一度掴んだらアンカーは固定

	if (bPulling)

	{

		return;

	}

	FHitResult Hit;

	Hit.bBlockingHit = true;

	Hit.ImpactPoint = AimTransform.GetLocation();

	Hit.Location = AimTransform.GetLocation();

	Hit.ImpactNormal =

		AimTransform

		.GetRotation()

		.GetUpVector()

		.GetSafeNormal();

	Hit.Normal = Hit.ImpactNormal;

	UE_LOG(

		LogTemp,

		Warning,

		TEXT("[GRAPPLE] PRESS -> START PULL")

	);

	StartPull(Hit);

}

bool USavaGrappleAbility::ComputeGrappleAim(

	FTransform& OutTransform,

	TArray<FVector>& OutPathPoints) const

{

	OutPathPoints.Reset();

	// 親変更前から使っていたグラップル判定をそのまま使用

	FHitResult Hit;

	if (!FindAnchor(Hit))

	{

		return false;

	}

	FVector ViewLocation;

	FVector Direction;

	if (!GetAim(ViewLocation, Direction))

	{

		return false;

	}

	UE_LOG(

		LogTemp,

		Warning,

		TEXT("[GRAPPLE] Anchor Found Actor=%s Point=%s Normal=%s"),

		Hit.GetActor()

		? *Hit.GetActor()->GetName()

		: TEXT("NULL"),

		*Hit.ImpactPoint.ToString(),

		*Hit.ImpactNormal.ToString()

	);

	// 壁の法線をTransformに保存

	const FQuat SurfaceRotation =

		FRotationMatrix::MakeFromZ(

			Hit.ImpactNormal.GetSafeNormal()

		).ToQuat();

	OutTransform = FTransform(

		SurfaceRotation,

		Hit.ImpactPoint

	);

	OutPathPoints.Add(ViewLocation);

	OutPathPoints.Add(Hit.ImpactPoint);

	return true;

}
void USavaGrappleAbility::OnAimStarted_Implementation()
{
	UE_LOG(LogTemp, Error, TEXT("[GRAPPLE][01] OnAimStarted ENTER"));
	bEnding = false;
	PullSourceID = 0;
	bPulling = false;
	bWaitingForTarget = false;
	Elapsed = 0.0f;
	StalledTime = 0.0f;
	USavaCharacterMovementComponent* Movement =
		GetSavaMovementFromActorInfo();
	UE_LOG(
		LogTemp,
		Error,
		TEXT("[GRAPPLE][02] Movement=%s Mode=%d Mantling=%d Active=%d"),
		Movement ? TEXT("OK") : TEXT("NULL"),
		Movement ? static_cast<int32>(Movement->MovementMode) : -1,
		Movement ? Movement->IsMantling() : 0,
		IsActive() ? 1 : 0
	);
	if (!Movement ||
		Movement->MovementMode == MOVE_None ||
		Movement->IsMantling())
	{
		UE_LOG(LogTemp, Error, TEXT("[GRAPPLE][FAIL-01] Invalid Movement"));
		Finish(true);
		return;
	}
	auto* DeathTask =
		UAbilityTask_WaitGameplayTagAdded::WaitGameplayTagAdd(
			this,
			SavaGameplayTags::State_Dead
		);
	DeathTask->Added.AddDynamic(this, &ThisClass::OnDied);
	DeathTask->ReadyForActivation();
	UE_LOG(LogTemp, Error, TEXT("[GRAPPLE][03] DeathTask started"));
	if (!IsActive())
	{
		UE_LOG(LogTemp, Error, TEXT("[GRAPPLE][FAIL-02] Ability became inactive"));
		return;
	}
	auto* TickTask =
		USavaAbilityTask_Tick::CreateTickTask(this);
	TickTask->OnTick.AddUObject(this, &ThisClass::TickPull);
	TickTask->ReadyForActivation();
	UE_LOG(LogTemp, Error, TEXT("[GRAPPLE][04] TickTask started"));
}
void USavaGrappleAbility::OnAimEnded_Implementation(
	bool bWasCancelled)
{
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("GRAPPLE BUTTON RELEASED")
	);
	if (bPulling)
	{
		Finish(false);
	}
}
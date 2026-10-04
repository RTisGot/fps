#include "SavaGrappleRootMotionSource.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SavaCharacterMovementComponent.h"

FRootMotionSource* FSavaGrappleRootMotionSource::Clone() const { return new FSavaGrappleRootMotionSource(*this); }
UScriptStruct* FSavaGrappleRootMotionSource::GetScriptStruct() const { return StaticStruct(); }

bool FSavaGrappleRootMotionSource::Matches(const FRootMotionSource* Other) const
{
	if (!FRootMotionSource_MoveToForce::Matches(Other)) return false;
	const auto* Source = static_cast<const FSavaGrappleRootMotionSource*>(Other);
	return PullSpeed == Source->PullSpeed && SteeringSpeed == Source->SteeringSpeed
		&& MaxLateralOffset == Source->MaxLateralOffset && SteeringResponse == Source->SteeringResponse
		&& JumpLiftSpeed == Source->JumpLiftSpeed;
}

bool FSavaGrappleRootMotionSource::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	if (!FRootMotionSource_MoveToForce::NetSerialize(Ar, Map, bOutSuccess)) return false;
	Ar << PullSpeed << SteeringSpeed << MaxLateralOffset << SteeringResponse;
	Ar << JumpLiftSpeed;
	bOutSuccess = !Ar.IsError();
	return bOutSuccess;
}

FVector FSavaGrappleRootMotionSource::CalculateVelocity(const FVector& Location,
	const FVector& PreviousVelocity, const FVector& NormalizedInput, float DeltaTime, bool bJumpHeld) const
{
	if (DeltaTime <= UE_SMALL_NUMBER) return FVector::ZeroVector;
	const FVector ToTarget = TargetLocation - Location;
	const float Distance = ToTarget.Size();
	if (Distance <= 1.0f) return FVector::ZeroVector;
	const FVector PullDirection = ToTarget / Distance;
	// Only the tangent component steers. Input cannot turn off/reverse the pull.
	const float ArrivalScale = FMath::Clamp((Distance - 35.0f) / 300.0f, 0.0f, 1.0f);
	const float MaxSteerSpeed = FMath::Max(SteeringSpeed, 0.0f) * ArrivalScale;
	const float LiftSpeed = FMath::Max(JumpLiftSpeed, 0.0f) * ArrivalScale;
	const float MaxTangentSpeed = FMath::Sqrt(FMath::Square(MaxSteerSpeed) + FMath::Square(LiftSpeed));
	// Upward tangent lift bows the path upward without detaching or reversing the pull.
	const FVector Lift = bJumpHeld ? FVector::VectorPlaneProject(FVector::UpVector, PullDirection) * LiftSpeed : FVector::ZeroVector;
	const FVector DesiredSteer = (FVector::VectorPlaneProject(NormalizedInput.GetClampedToMaxSize(1.0f), PullDirection) * MaxSteerSpeed + Lift).GetClampedToMaxSize(MaxTangentSpeed);
	const FVector OldSteer = FVector::VectorPlaneProject(PreviousVelocity, PullDirection).GetClampedToMaxSize(MaxTangentSpeed);
	const float Blend = 1.0f - FMath::Exp(-FMath::Max(SteeringResponse, 0.0f) * DeltaTime);
	const FVector Steer = FMath::Lerp(OldSteer, DesiredSteer, Blend).GetClampedToMaxSize(MaxTangentSpeed);
	const float ForwardSpeed = FMath::Min(FMath::Max(PullSpeed, 0.0f), Distance / DeltaTime);
	FVector Delta = (PullDirection * ForwardSpeed + Steer) * DeltaTime;

	// Constrain deviation from the original line without moving the fixed rope anchor.
	const FVector Axis = (TargetLocation - StartLocation).GetSafeNormal();
	if (!Axis.IsNearlyZero())
	{
		const FVector Next = Location + Delta;
		const FVector OnLine = StartLocation + Axis * FVector::DotProduct(Next - StartLocation, Axis);
		const FVector Offset = Next - OnLine;
		Delta = OnLine + Offset.GetClampedToMaxSize(FMath::Max(MaxLateralOffset, 0.0f)) - Location;
	}
	return (Delta / DeltaTime).GetClampedToMaxSize(FMath::Sqrt(FMath::Square(FMath::Max(PullSpeed, 0.0f)) + FMath::Square(MaxTangentSpeed)));
}

void FSavaGrappleRootMotionSource::PrepareRootMotion(float SimulationTime, float MovementTickTime,
	const ACharacter& Character, const UCharacterMovementComponent& MoveComponent)
{
	RootMotionParams.Clear();
	if (SimulationTime > UE_SMALL_NUMBER && MovementTickTime > UE_SMALL_NUMBER)
	{
		// Acceleration is already part of CMC ServerMove and saved-move replay.
		const FVector Input = MoveComponent.GetCurrentAcceleration() / FMath::Max(MoveComponent.GetMaxAcceleration(), 1.0f);
		const auto* SavaMovement = Cast<USavaCharacterMovementComponent>(&MoveComponent);
		const bool bLift = SavaMovement && SavaMovement->IsJumpHeld();
		const FVector Velocity = CalculateVelocity(Character.GetActorLocation(), MoveComponent.Velocity, Input, SimulationTime, bLift);
		RootMotionParams.Set(FTransform(Velocity * (SimulationTime / MovementTickTime)));
	}
	SetTime(GetTime() + SimulationTime);
}

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/RootMotionSource.h"
#include "SavaGrappleRootMotionSource.generated.h"

/** Uses CharacterMovement's saved/replicated acceleration, not unsynchronized ability ticks. */
USTRUCT()
struct SAVA_API FSavaGrappleRootMotionSource : public FRootMotionSource_MoveToForce
{
	GENERATED_BODY()

	UPROPERTY()
	float PullSpeed = 2000.0f;
	UPROPERTY()
	float SteeringSpeed = 1200.0f;
	UPROPERTY()
	float MaxLateralOffset = 700.0f;
	UPROPERTY()
	float SteeringResponse = 12.0f;
	UPROPERTY()
	float JumpLiftSpeed = 1000.0f;

	virtual FRootMotionSource* Clone() const override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual bool Matches(const FRootMotionSource* Other) const override;
	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
	virtual void PrepareRootMotion(float SimulationTime, float MovementTickTime,
		const ACharacter& Character, const UCharacterMovementComponent& MoveComponent) override;

	FVector CalculateVelocity(const FVector& Location, const FVector& PreviousVelocity,
		const FVector& NormalizedInput, float DeltaTime, bool bJumpHeld = false) const;
};

template<>
struct TStructOpsTypeTraits<FSavaGrappleRootMotionSource> : public TStructOpsTypeTraitsBase2<FSavaGrappleRootMotionSource>
{
	enum { WithNetSerializer = true, WithCopy = true };
};

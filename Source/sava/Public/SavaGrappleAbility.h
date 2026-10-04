#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/SavaGameplayAbility.h"
#include "SavaGrappleAbility.generated.h"

class ASavaGrappleRope;
struct FGameplayAbilityTargetDataHandle;

/** Hold the existing grapple input to pull toward a static surface. */
UCLASS()
class SAVA_API USavaGrappleAbility : public USavaGameplayAbility
{
	GENERATED_BODY()

public:
	USavaGrappleAbility();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Grapple", meta = (ClampMin = "100", Units = "cm"))
	float MaxRange = 3000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Grapple", meta = (ClampMin = "100", Units = "cm/s"))
	float PullSpeed = 2000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Grapple|Steering", meta = (ClampMin = "0", Units = "cm/s"))
	float SteeringSpeed = 1200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Grapple|Steering", meta = (ClampMin = "0", Units = "cm"))
	float MaxLateralOffset = 700.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Grapple|Steering", meta = (ClampMin = "0.1"))
	float SteeringResponse = 12.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Grapple|Steering", meta = (ClampMin = "0", Units = "cm/s"))
	float JumpLiftSpeed = 1000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Grapple", meta = (ClampMin = "0.1", Units = "s"))
	float MaxPullDuration = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Grapple", meta = (ClampMin = "1", Units = "cm"))
	float SurfaceClearance = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Grapple")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** Replace with a Blueprint child to customize the rope mesh/material. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Grapple")
	TSubclassOf<ASavaGrappleRope> RopeClass;

	UFUNCTION(BlueprintImplementableEvent, Category = "Sava|Grapple")
	void OnGrappleStarted(FVector AnchorLocation);

	UFUNCTION(BlueprintImplementableEvent, Category = "Sava|Grapple")
	void OnGrappleFinished(bool bWasCancelled);

private:
	bool GetAim(FVector& Location, FVector& Direction) const;
	bool FindAnchor(FHitResult& Hit) const;
	bool IsAnchorAllowed(const FHitResult& Hit) const;
	void ReceiveTargetData(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag);
	void StartPull(const FHitResult& Hit);
	void TickPull(float DeltaTime);
	void Finish(bool bCancelled);

	UFUNCTION()
	void OnReleased(float TimeHeld);
	UFUNCTION()
	void OnDied();

	UPROPERTY(Transient)
	TObjectPtr<ASavaGrappleRope> Rope;

	FVector TargetLocation = FVector::ZeroVector;
	FVector LastLocation = FVector::ZeroVector;
	float Elapsed = 0.0f;
	float StalledTime = 0.0f;
	bool bPulling = false;
	bool bWaitingForTarget = false;
	bool bEnding = false;
	uint16 PullSourceID = 0;
};

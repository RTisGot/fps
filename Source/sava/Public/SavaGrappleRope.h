#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SavaGrappleRope.generated.h"

class UStaticMeshComponent;

/** Straight, taut rope for the pull grapple. A Blueprint child can customize its material. */
UCLASS(Blueprintable)
class SAVA_API ASavaGrappleRope : public AActor
{
	GENERATED_BODY()
public:
	ASavaGrappleRope();
	void InitializeAnchor(const FVector& Location);
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grapple")
	TObjectPtr<UStaticMeshComponent> RopeMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grapple", meta = (ClampMin = "0.1"))
	float RopeDiameter = 2.0f;

	UPROPERTY(Replicated)
	FVector_NetQuantize Anchor = FVector::ZeroVector;
};

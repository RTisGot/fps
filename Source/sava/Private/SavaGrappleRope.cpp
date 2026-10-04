#include "SavaGrappleRope.h"

#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "savaCharacter.h"

ASavaGrappleRope::ASavaGrappleRope()
{
	bReplicates = true;
	bNetUseOwnerRelevancy = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	RopeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RopeMesh"));
	SetRootComponent(RopeMesh);
	RopeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RopeMesh->SetGenerateOverlapEvents(false);
	RopeMesh->SetCastShadow(false);
	RopeMesh->SetCanEverAffectNavigation(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cylinder.Succeeded()) RopeMesh->SetStaticMesh(Cylinder.Object);
	RopeMesh->SetVisibility(false);
}

void ASavaGrappleRope::InitializeAnchor(const FVector& Location)
{
	Anchor = Location;
	ForceNetUpdate();
	Tick(0.0f);
}

void ASavaGrappleRope::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const AsavaCharacter* Character = Cast<AsavaCharacter>(GetOwner());
	if (!IsValid(Character))
	{
		RopeMesh->SetVisibility(false);
		if (HasAuthority()) Destroy();
		return;
	}
	// Locally predicted actor has authority locally. Hide only the server's duplicate on its owner.
	if (!HasAuthority() && Character->IsLocallyControlled())
	{
		RopeMesh->SetVisibility(false);
		return;
	}
	FVector Start = Character->GetActorLocation() + FVector(0, 0, 30);
	if (Character->IsLocallyControlled())
	{
		const UCameraComponent* Camera = Character->GetFirstPersonCameraComponent();
		Start = Camera->GetComponentLocation() + Camera->GetForwardVector() * 35.0f
			+ Camera->GetRightVector() * 18.0f - Camera->GetUpVector() * 18.0f;
	}
	const FVector Segment = FVector(Anchor) - Start;
	RopeMesh->SetVisibility(true);
	SetActorLocationAndRotation(Start + Segment * 0.5f, FRotationMatrix::MakeFromZ(Segment).Rotator());
	// Engine cylinder is 100 cm tall and 100 cm in diameter.
	SetActorScale3D(FVector(RopeDiameter / 100.0f, RopeDiameter / 100.0f, Segment.Size() / 100.0f));
}

void ASavaGrappleRope::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASavaGrappleRope, Anchor);
}

// Fill out your copyright notice in the Description page of Project Settings.


#include "SavaCharacterMovementComponent.h"
#include "GameFramework/Character.h"

bool USavaCharacterMovementComponent::IsSprinting() const
{
	if(!bWantsToSprint || !IsMovingOnGround() || IsCrouchIng() || !UpdatedComponent)
	{
		return false;
	}

	const FVector InputDir = FVector(Acceleration.X, Acceleration.Y,0.0f).GetSafeNormal();
	if (InputDir.IsNearlyZero()) {
		return false;
	}

	const FVector Forward = UpdatedComponent->GetForwardVector().GetSafeNormal2D();
	return FVector::DotProduct(InputDir, Forward) >= FMath::Cos(FMath::DegreesToRadians(SprintMaxAngle));
}

float USavaCharacterMovementComponent::GetMaxSpeed() const {
	if (isSprinting()) {
		return MaxSpeed;
	}
	return Super::GetMaxSpeed();
}

void USavaCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);
	bWantsToSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
}

// Network Prediction
FNetworkPredictionData_Client* USavaCharacterMovementComponent::GetPredictionData_Client() const {
	check(PawnOwner != nullptr);

	if (!ClientPredictionData)
	{
		USavaCharacterMovementComponent* MutableThis = const_cast<USavaCharacterMovementComponent*>(this);
	}
	return ClientPredictionData;
}

USavaCharacterMovementComponent::FNetworkPredictionData_Client_Sava::FNetworkPredictionData_Client_Sava(const UCharacterMovementComponent& ClientMovement)
	: Super(ClientMovement)
{
}

FSavedMovePtr USavaCharacterMovementComponent::FNetworkPredictionData_Client_Sava::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_Sava());
}

void USavaCharacterMovementComponent::FSavedMove_Sava::Clear()
{
	Super::Clear();
	bSavedWantsToSprint = false;
}

uint8 USavaCharacterMovementComponent::FSavedMove_Sava::GetCompressedFlags() const
{
	uint8 Result = Super::GetCompressedFlags();
	if (bSavedWantsToSprint)
	{
		Result |= FSavedMove_Character::FLAG_Custom_0;
	}
	return Result;
}

bool USavaCharacterMovementComponent::FSavedMove_Sava::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const
{
	if (bSavedWantsToSprint != static_cast<FSavedMove_Sava*>(NewMove.Get())->bSavedWantsToSprint) {
		return false;
	}
	return Super::CanCombineWith(NewMove, Character, MaxDelta);
}

void USavaCharacterMovementComponent::FSavedMove_Sava::SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(Character, InDeltaTime, NewAccel, ClientData);
	USavaCharacterMovementComponent* SavaMovement = Cast<USavaCharacterMovementComponent>(Character->GetCharacterMovement());
	if (SavaMovement) {
		bSavedWantsToSprint = SavaMovement->bWantsToSprint;
	}
}

void USavaCharacterMovementComponent::FSavedMove_Sava::PrepMoveFor(class ACharacter* Character)
{
	Super::PrepMoveFor(Character);
	USavaCharacterMovementComponent* SavaMovement = Cast<USavaCharacterMovementComponent>(Character->GetCharacterMovement());
	if (SavaMovement) {
		SavaMovement->bWantsToSprint = bSavedWantsToSprint;
	}
}
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SavaCharacterMovementComponent.generated.h"

UCLASS()
class SAVA_API USavaCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

	class FSavedMove_Sava : public FSavedMove_Character
	{
	public:
		typedef FSavedMove_Character Super;

		uint8 bSavedWantsToSprint : 1;

		virtual void Clear() override;
		virtual uint8 GetCompressedFlags() const override;
		virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const override;
		virtual void SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character& ClientData) override;
		virtual void PrepMoveFor(class ACharacter* Character) override;
	};


	class FNetworkPredictionData_Client_Sava : public FNetworkPredictionData_Client_Character
	{
	public:
		typedef FNetworkPredictionData_Client_Character Super;
		FNetworkPredictionData_Client_Sava(const UCharacterMovementComponent& ClientMovement);
		virtual FSavedMovePtr AllocateNewMove() override;
	};

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Sprint", meta = (ClampMin = "0", ForceUnits = "cm/s"))
		float SprintSpeed = 650.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Sprint", meta = (ClampMin = "0", ClampMax = "90", ForceUnits = "Deg"))
		float SprintMaxAngle = 50.0f;

	UFUNCTION(BlueprintCallable,Category = "Sava|Sprint")
	void StartSprint() { bWantsToSprint = true; }
	UFUNCTION(BlueprintCallable, Category = "Sava|Sprint")
	void StopSprint() { bWantsToSprint = false; }

	UFUNCTION(BlueprintPure, Category = "Sava|Sprint")
	bool IsSprinting() const;

	virtual float GetMaxSpeed() const override;
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;

protected:
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;

private:
	bool bWantsToSprint = false;
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SavaCharacterMovementComponent.generated.h"

//MOVE_Custom のサブモード
enum ESavaCustomMovementMode : uint8
{
	CMOVE_None = 0,
	CMOVE_Slide,
};

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
		float SprintSpeedMultiplier = 1.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Sprint", meta = (ClampMin = "0", ClampMax = "90", ForceUnits = "Deg"))
		float SprintMaxAngle = 50.0f;

	UFUNCTION(BlueprintCallable,Category = "Sava|Sprint")
	void StartSprint() { bWantsToSprint = true; }
	UFUNCTION(BlueprintCallable, Category = "Sava|Sprint")
	void StopSprint() { bWantsToSprint = false; }

	UFUNCTION(BlueprintPure, Category = "Sava|Sprint")
	bool IsSprinting() const;

	USavaCharacterMovementComponent();

	//スライディング開始に必要な水平速度(歩きより速く、ダッシュより遅い値にする)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Slide", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float SlideMinStartSpeed = 500.0f;

	//これより遅くなったらスライディング終了
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Slide", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float SlideMinSpeed = 250.0f;

	//開始時に進行方向へ加える速度
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Slide", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float SlideEnterImpulse = 300.0f;

	//スライディング中の最高速度(下り坂での加速の上限)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Slide", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float SlideMaxSpeed = 1500.0f;

	//速度に比例する減速(大きいほど早く止まる)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Slide", meta = (ClampMin = "0"))
	float SlideFriction = 0.6f;

	//速度に関係なく一定の減速
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Slide", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float SlideBrakingDeceleration = 200.0f;

	//坂道で重力の影響を受ける強さ(1=そのまま, 0=坂の影響なし)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Slide", meta = (ClampMin = "0"))
	float SlideGravityScale = 1.0f;

	//左右入力で進行方向を曲げる強さ
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sava|Slide", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float SlideSteerAcceleration = 400.0f;

	UFUNCTION(BlueprintPure, Category = "Sava|Slide")
	bool IsSliding() const;

	virtual bool IsMovingOnGround() const override;
	virtual bool CanAttemptJump() const override;
	virtual float GetMaxSpeed() const override;
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;

protected:
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;

private:
	void EnterSlide();
	void PhysSlide(float DeltaTime, int32 Iterations);

	bool bWantsToSprint = false;
};

// Fill out your copyright notice in the Description page of Project Settings.


#include "SavaCharacterMovementComponent.h"
#include "GameFramework/Character.h"

USavaCharacterMovementComponent::USavaCharacterMovementComponent()
{
	//しゃがみを有効化(初期値は false で Crouch() が何もしない)
	NavAgentProps.bCanCrouch = true;
}

bool USavaCharacterMovementComponent::IsSprinting() const
{
	if(!bWantsToSprint || !IsMovingOnGround() || IsCrouching() || !UpdatedComponent)
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
	if (IsSliding()) {
		return SlideMaxSpeed;
	}
	if (IsSprinting()) {
		return SprintSpeed;
	}
	return Super::GetMaxSpeed();
}

void USavaCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);
	bWantsToSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
}

//--------------------------------Slide

bool USavaCharacterMovementComponent::IsSliding() const
{
	return MovementMode == MOVE_Custom && CustomMovementMode == CMOVE_Slide;
}

bool USavaCharacterMovementComponent::IsMovingOnGround() const
{
	//スライディングも「地上」として扱う(しゃがみ維持・床の判定などに使われる)
	return Super::IsMovingOnGround() || (IsSliding() && UpdatedComponent);
}

bool USavaCharacterMovementComponent::CanAttemptJump() const
{
	//通常はしゃがみボタン押下中はジャンプ不可だが、スライディング中は許可する
	if (IsSliding())
	{
		return IsJumpAllowed();
	}
	return Super::CanAttemptJump();
}

void USavaCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	//他プレイヤー(SimulatedProxy)はサーバーから複製された状態をそのまま使う
	const bool bCanChangeState = CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy;
	const bool bWasCrouching = IsCrouching();

	//しゃがみボタンを離したらスライディング終了(立ち上がりは下の Super が行う)
	if (bCanChangeState && IsSliding() && !bWantsToCrouch)
	{
		SetMovementMode(MOVE_Walking);
	}

	//しゃがみ / 立ち上がり
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);

	//このフレームでしゃがんだ + 十分な速度 → スライディング開始
	if (bCanChangeState && !bWasCrouching && IsCrouching() && MovementMode == MOVE_Walking
		&& Velocity.SizeSquared2D() >= FMath::Square(SlideMinStartSpeed))
	{
		EnterSlide();
	}
}

void USavaCharacterMovementComponent::EnterSlide()
{
	Velocity += Velocity.GetSafeNormal2D() * SlideEnterImpulse;
	SetMovementMode(MOVE_Custom, CMOVE_Slide);

	//地面にいるので、しゃがみ/立ち上がりは足元を基準にする(Custom モードでは既定で false になる)
	bCrouchMaintainsBaseLocation = true;
	FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, false);
}

void USavaCharacterMovementComponent::PhysCustom(float DeltaTime, int32 Iterations)
{
	if (CustomMovementMode == CMOVE_Slide)
	{
		PhysSlide(DeltaTime, Iterations);
	}
	Super::PhysCustom(DeltaTime, Iterations);
}

void USavaCharacterMovementComponent::PhysSlide(float DeltaTime, int32 Iterations)
{
	if (DeltaTime < MIN_TICK_TIME)
	{
		return;
	}

	//足元に歩ける床がなければ落下へ
	FFindFloorResult Floor;
	FindFloor(UpdatedComponent->GetComponentLocation(), Floor, false);
	if (!Floor.IsWalkableFloor())
	{
		SetMovementMode(MOVE_Falling);
		StartNewPhysics(DeltaTime, Iterations);
		return;
	}
	const FVector FloorNormal = Floor.HitResult.ImpactNormal;

	//坂道: 重力の斜面方向の成分で加速(下り) / 減速(上り)
	const FVector Gravity(0.0f, 0.0f, GetGravityZ());
	Velocity += FVector::VectorPlaneProject(Gravity, FloorNormal) * SlideGravityScale * DeltaTime;

	//左右入力で進行方向を少し曲げる(速さは変えない)
	const FVector SteerDir = FVector::VectorPlaneProject(Acceleration, Velocity.GetSafeNormal()).GetSafeNormal2D();
	if (!SteerDir.IsNearlyZero())
	{
		const float CurrentSpeed = Velocity.Size();
		Velocity = (Velocity + SteerDir * SlideSteerAcceleration * DeltaTime).GetSafeNormal() * CurrentSpeed;
	}

	//減速: 速度比例の摩擦 + 一定の制動
	const float Speed = Velocity.Size();
	const float NewSpeed = FMath::Max(Speed - (SlideFriction * Speed + SlideBrakingDeceleration) * DeltaTime, 0.0f);
	Velocity = FVector::VectorPlaneProject(Velocity.GetSafeNormal() * NewSpeed, FloorNormal).GetClampedToMaxSize(SlideMaxSpeed);

	//遅くなったら終了(ボタンを押し続けていればしゃがみ歩きになる)
	if (Velocity.SizeSquared() < FMath::Square(SlideMinSpeed))
	{
		SetMovementMode(MOVE_Walking);
		StartNewPhysics(DeltaTime, Iterations);
		return;
	}

	//移動
	Iterations++;
	bJustTeleported = false;
	const FVector OldLocation = UpdatedComponent->GetComponentLocation();
	const FVector Delta = Velocity * DeltaTime;
	FHitResult Hit(1.0f);
	SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);

	if (Hit.Time < 1.0f)
	{
		//段差なら登る。登れなければ壁に沿って滑る
		const FVector RemainingDelta = Delta * (1.0f - Hit.Time);
		if (IsWalkable(Hit) || !CanStepUp(Hit) || !StepUp(GetGravityDirection(), RemainingDelta, Hit))
		{
			HandleImpact(Hit, DeltaTime, Delta);
			SlideAlongSurface(Delta, 1.0f - Hit.Time, Hit.Normal, Hit, true);
		}
	}

	//床に吸着させる(下り坂や凹凸で浮かないように)
	FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, false);
	if (CurrentFloor.IsWalkableFloor())
	{
		AdjustFloorHeight();
		SetBaseFromFloor(CurrentFloor);
	}

	//実際に動いた量から速度を更新(壁に当たって止まった分などを反映)
	if (!bJustTeleported)
	{
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / DeltaTime;
	}

	if (!CurrentFloor.IsWalkableFloor())
	{
		SetMovementMode(MOVE_Falling);
	}
}

// Network Prediction
FNetworkPredictionData_Client* USavaCharacterMovementComponent::GetPredictionData_Client() const {
	check(PawnOwner != nullptr);

	if (!ClientPredictionData)
	{
		USavaCharacterMovementComponent* MutableThis = const_cast<USavaCharacterMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_Sava(*this);
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
#include "SavaCharacterMovementComponent.h"

#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"

// ---------------------------------------------------------------------------
// SavedMove：1フレーム分の入力と結果の記録。
// クライアントは自分の予測で先に動き、この記録をサーバーへ送る。サーバーの結果と
// ずれていたら、記録した入力を最初からやり直して位置を合わせ直す（リプレイ）。
// 独自の入力（ダッシュ）はここに含めないと、やり直しのときに再現できない。
// ---------------------------------------------------------------------------
class FSavedMove_Sava : public FSavedMove_Character
{
public:
	typedef FSavedMove_Character Super;

	uint8 bSavedWantsToSprint : 1;

	FSavedMove_Sava()
		: bSavedWantsToSprint(0)
	{
	}

	virtual void Clear() override
	{
		Super::Clear();
		bSavedWantsToSprint = 0;
	}

	// サーバーへ送るビット。移動データは帯域を節約するため、フラグは1ビットずつ詰める
	virtual uint8 GetCompressedFlags() const override
	{
		uint8 Result = Super::GetCompressedFlags();
		if (bSavedWantsToSprint)
		{
			Result |= FLAG_Custom_0;
		}
		return Result;
	}

	// 入力が同じフレームはまとめて送られる。ダッシュの状態が違うならまとめてはいけない
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override
	{
		const FSavedMove_Sava* Other = static_cast<const FSavedMove_Sava*>(NewMove.Get());
		if (bSavedWantsToSprint != Other->bSavedWantsToSprint)
		{
			return false;
		}

		return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
	}

	// 移動する直前：今の入力を記録する
	virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override
	{
		Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);

		if (const USavaCharacterMovementComponent* Movement = Cast<USavaCharacterMovementComponent>(C->GetCharacterMovement()))
		{
			bSavedWantsToSprint = Movement->WantsToSprint();
		}
	}

	// やり直しの直前：記録した入力をコンポーネントへ戻す
	virtual void PrepMoveFor(ACharacter* C) override
	{
		Super::PrepMoveFor(C);

		if (USavaCharacterMovementComponent* Movement = Cast<USavaCharacterMovementComponent>(C->GetCharacterMovement()))
		{
			Movement->SetWantsToSprint(bSavedWantsToSprint != 0);
		}
	}
};

// SavedMove を独自クラスに差し替えるための入れ物
class FNetworkPredictionData_Client_Sava : public FNetworkPredictionData_Client_Character
{
public:
	typedef FNetworkPredictionData_Client_Character Super;

	explicit FNetworkPredictionData_Client_Sava(const UCharacterMovementComponent& ClientMovement)
		: Super(ClientMovement)
	{
	}

	virtual FSavedMovePtr AllocateNewMove() override
	{
		return FSavedMovePtr(new FSavedMove_Sava());
	}
};

// ---------------------------------------------------------------------------

USavaCharacterMovementComponent::USavaCharacterMovementComponent()
{
	MaxWalkSpeed = 450.f;
	MaxWalkSpeedCrouched = 250.f;
	BrakingDecelerationWalking = 2048.f;

	// しゃがみを有効にする。これが false だと Crouch() を呼んでも何も起きない
	GetNavAgentPropertiesRef().bCanCrouch = true;
	SetCrouchedHalfHeight(60.f);
}

FNetworkPredictionData_Client* USavaCharacterMovementComponent::GetPredictionData_Client() const
{
	if (ClientPredictionData == nullptr)
	{
		USavaCharacterMovementComponent* MutableThis = const_cast<USavaCharacterMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_Sava(*this);
	}

	return ClientPredictionData;
}

// サーバー側：クライアントから届いたビットを入力へ戻す
void USavaCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	bWantsToSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
}

bool USavaCharacterMovementComponent::IsSprinting() const
{
	if (!bWantsToSprint || MovementMode != MOVE_Walking || IsCrouching() || !UpdatedComponent)
	{
		return false;
	}

	// 前方向の入力のときだけダッシュできる
	const FVector InputDirection = Acceleration.GetSafeNormal2D();
	const FVector Forward = UpdatedComponent->GetForwardVector().GetSafeNormal2D();

	return FVector::DotProduct(InputDirection, Forward) >= SprintForwardThreshold;
}

bool USavaCharacterMovementComponent::IsSliding() const
{
	return MovementMode == MOVE_Custom && CustomMovementMode == static_cast<uint8>(ESavaMovementMode::Slide);
}

float USavaCharacterMovementComponent::GetMaxSpeed() const
{
	if (IsSprinting())
	{
		return MaxSprintSpeed;
	}

	if (IsSliding())
	{
		return SlideMaxStartSpeed;
	}

	return Super::GetMaxSpeed();
}

// スライディング中も「地面の上にいる」扱いにする。
// エンジンはこの判定でしゃがみの可否や床の処理を決めているため、
// これを返さないとスライディング中に強制的に立ち上がってしまう。
bool USavaCharacterMovementComponent::IsMovingOnGround() const
{
	return Super::IsMovingOnGround() || IsSliding();
}

// エンジンは「しゃがみ入力中はジャンプ不可」。スライディング中だけは許可する
bool USavaCharacterMovementComponent::CanAttemptJump() const
{
	if (IsSliding())
	{
		return IsJumpAllowed();
	}

	return Super::CanAttemptJump();
}

void USavaCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	// しゃがみを離したらスライディングを終える（Super より先に。歩きに戻してから立たせる）
	if (IsSliding() && !bWantsToCrouch)
	{
		SetMovementMode(MOVE_Walking);
	}

	// エンジン標準のしゃがみ／立ち上がり処理
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);

	if (CanStartSlide())
	{
		StartSlide();
	}
}

bool USavaCharacterMovementComponent::CanStartSlide() const
{
	// ダッシュキーとしゃがみキーを押していて、十分速く、地面を歩いていること。
	// 入力と速度だけで判定しているので、クライアントとサーバーで必ず同じ結果になる
	return MovementMode == MOVE_Walking
		&& bWantsToSprint
		&& bWantsToCrouch
		&& IsCrouching()
		&& Velocity.SizeSquared2D() >= FMath::Square(SlideMinStartSpeed);
}

void USavaCharacterMovementComponent::StartSlide()
{
	const float CurrentSpeed = Velocity.Size2D();
	const float BoostedSpeed = FMath::Min(CurrentSpeed + SlideEnterBoost, SlideMaxStartSpeed);

	Velocity = Velocity.GetSafeNormal2D() * FMath::Max(CurrentSpeed, BoostedSpeed);

	SetMovementMode(MOVE_Custom, static_cast<uint8>(ESavaMovementMode::Slide));
}

void USavaCharacterMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

	if (IsSliding())
	{
		// Super はカスタムモードに入るとき床の情報を捨てるので、拾い直す
		bCrouchMaintainsBaseLocation = true;
		FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, false);
		SetBaseFromFloor(CurrentFloor);
	}
}

void USavaCharacterMovementComponent::PhysCustom(float DeltaTime, int32 Iterations)
{
	if (CustomMovementMode == static_cast<uint8>(ESavaMovementMode::Slide))
	{
		PhysSlide(DeltaTime, Iterations);
		return;
	}

	Super::PhysCustom(DeltaTime, Iterations);
}

// スライディング中の1フレーム分の移動。PhysWalking の簡易版
void USavaCharacterMovementComponent::PhysSlide(float DeltaTime, int32 Iterations)
{
	if (DeltaTime < MIN_TICK_TIME)
	{
		return;
	}

	if (!CharacterOwner || !UpdatedComponent)
	{
		return;
	}

	// 床が無ければ落下へ
	if (!CurrentFloor.IsWalkableFloor())
	{
		SetMovementMode(MOVE_Falling);
		StartNewPhysics(DeltaTime, Iterations);
		return;
	}

	Iterations++;
	bJustTeleported = false;

	const FVector FloorNormal = CurrentFloor.HitResult.ImpactNormal;

	// 1. 坂道の重力：重力のうち床に沿う成分だけを足す（下り坂で加速、上り坂で減速）
	const FVector Gravity(0.f, 0.f, GetGravityZ());
	Velocity += FVector::VectorPlaneProject(Gravity, FloorNormal) * SlideGravityScale * DeltaTime;

	// 2. 摩擦：進行方向と逆向きに一定量だけ減速する
	const float SpeedBeforeFriction = Velocity.Size();
	Velocity -= Velocity.GetSafeNormal() * FMath::Min(SlideDeceleration * DeltaTime, SpeedBeforeFriction);

	// 3. 左右入力で曲がる。速度を足したあと元の速さに戻すので、曲がっても加速しない
	const float CurrentSpeed = Velocity.Size();
	const FVector MoveDirection = Velocity.GetSafeNormal();
	const float MaxAccel = GetMaxAcceleration();
	if (CurrentSpeed > 0.f && MaxAccel > 0.f)
	{
		const FVector Input = Acceleration / MaxAccel;                        // 長さ 0～1 の入力
		const FVector Lateral = Input - Input.ProjectOnTo(MoveDirection);     // 進行方向に垂直な成分だけ
		Velocity = (Velocity + Lateral * SlideSteerAcceleration * DeltaTime).GetSafeNormal() * CurrentSpeed;
	}

	// 床の面に沿った速度にそろえる
	Velocity = FVector::VectorPlaneProject(Velocity, FloorNormal);

	// 4. 実際に動かす
	const FVector OldLocation = UpdatedComponent->GetComponentLocation();
	const FVector Delta = Velocity * DeltaTime;
	FHitResult Hit(1.f);
	SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);

	if (Hit.IsValidBlockingHit())
	{
		const float TimeLeft = 1.f - Hit.Time;
		FStepDownResult StepDownResult;

		// 小さな段差は乗り越え、それ以外は壁に沿って滑る
		if (CanStepUp(Hit) && StepUp(GetGravityDirection(), Delta * TimeLeft, Hit, &StepDownResult))
		{
			if (StepDownResult.bComputedFloor)
			{
				CurrentFloor = StepDownResult.FloorResult;
			}
		}
		else
		{
			HandleImpact(Hit, DeltaTime, Delta);
			SlideAlongSurface(Delta, TimeLeft, Hit.Normal, Hit, true);
		}
	}

	// 5. 実際に動いた距離から速度を計算し直す（壁にぶつかったぶん遅くなる）
	if (!bJustTeleported)
	{
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / DeltaTime;
	}

	// 6. 床を探し直す
	FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, false);
	if (!CurrentFloor.IsWalkableFloor())
	{
		// 床が無くなった（崖から飛び出した）。速度は保ったまま落下する
		SetMovementMode(MOVE_Falling);
		return;
	}

	AdjustFloorHeight();
	SetBaseFromFloor(CurrentFloor);

	// 7. 遅くなったらしゃがみ歩きへ戻す
	if (Velocity.SizeSquared() < FMath::Square(SlideExitSpeed))
	{
		SetMovementMode(MOVE_Walking);
	}
}

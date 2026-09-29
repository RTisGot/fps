// Fill out your copyright notice in the Description page of Project Settings.


#include "SavaCharacterMovementComponent.h"
#include "AbilitySystem/SavaAttributeSet.h"
#include "AbilitySystemGlobals.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"

namespace
{
	//壁走り中、壁から離れないように壁へ押し付ける速さ
	constexpr float WallRunStickSpeed = 200.0f;
}

USavaCharacterMovementComponent::USavaCharacterMovementComponent()
{
	//しゃがみを有効化(初期値は false で Crouch() が何もしない)
	NavAgentProps.bCanCrouch = true;

	//補正時に独自の状態も送る
	SetMoveResponseDataContainer(SavaMoveResponseDataContainer);
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
	//スキル等による速度変化(加速・減速など)は GAS の MoveSpeedMultiplier で受け取る
	const float AbilitySpeedMultiplier = GetAbilityMoveSpeedMultiplier();
	if (IsSprinting()) {
		return Super::GetMaxSpeed() * SprintSpeedMultiplier * AbilitySpeedMultiplier;
	}
	return Super::GetMaxSpeed() * AbilitySpeedMultiplier;
}

float USavaCharacterMovementComponent::GetAbilityMoveSpeedMultiplier() const
{
	//サーバーとクライアントで同じ値になる(属性は複製・予測される)ので、補正の原因にならない
	const UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	if (!AbilitySystem)
	{
		return 1.0f;
	}

	bool bFound = false;
	const float Multiplier = AbilitySystem->GetGameplayAttributeValue(USavaAttributeSet::GetMoveSpeedMultiplierAttribute(), bFound);
	return bFound ? FMath::Max(Multiplier, 0.0f) : 1.0f;
}

void USavaCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);
	bWantsToSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
}

//--------------------------------Air

void USavaCharacterMovementComponent::PhysFalling(float DeltaTime, int32 Iterations)
{
	ApplyAirOverspeedDecay(DeltaTime);
	Super::PhysFalling(DeltaTime, Iterations);
}

void USavaCharacterMovementComponent::ApplyAirOverspeedDecay(float DeltaTime)
{
	//基準速度を超えた分だけを指数的に減らす(基準以下の通常ジャンプには影響しない)
	const float SoftCap = MaxWalkSpeed * AirSpeedSoftCapMultiplier;
	const float Speed = Velocity.Size2D();
	if (Speed <= SoftCap)
	{
		return;
	}

	const float Scale = ApplyOverspeedDecay(Speed, SoftCap, DeltaTime) / Speed;
	Velocity.X *= Scale;
	Velocity.Y *= Scale;
}

float USavaCharacterMovementComponent::ApplyOverspeedDecay(float Speed, float SoftCap, float DeltaTime) const
{
	if (Speed <= SoftCap)
	{
		return Speed;
	}
	return SoftCap + (Speed - SoftCap) * FMath::Exp(-AirOverspeedDecayRate * DeltaTime);
}

//--------------------------------WallRun

bool USavaCharacterMovementComponent::IsWallRunning() const
{
	return MovementMode == MOVE_Custom && CustomMovementMode == CMOVE_WallRun;
}

bool USavaCharacterMovementComponent::TraceMovementLine(const FVector& Start, const FVector& End, FHitResult& OutHit) const
{
	//キャラクターの移動と同じ当たり判定の設定で線を飛ばす
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SavaWallRun), false, CharacterOwner);
	FCollisionResponseParams ResponseParams;
	InitCollisionParams(Params, ResponseParams);
	return GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, UpdatedComponent->GetCollisionObjectType(), Params, ResponseParams);
}

bool USavaCharacterMovementComponent::FindWallRunWall(const FVector& Direction, FHitResult& OutHit) const
{
	const float Radius = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const FVector Start = UpdatedComponent->GetComponentLocation();
	const FVector End = Start + Direction.GetSafeNormal2D() * (Radius + WallRunCheckDistance);
	return TraceMovementLine(Start, End, OutHit) && IsValidWallRunWall(OutHit);
}

bool USavaCharacterMovementComponent::IsValidWallRunWall(const FHitResult& Hit) const
{
	//ほぼ垂直な面だけ。他のプレイヤーには張り付かない
	return Hit.bBlockingHit
		&& FMath::Abs(Hit.ImpactNormal.Z) <= WallRunMaxWallNormalZ
		&& !Cast<APawn>(Hit.GetActor());
}

bool USavaCharacterMovementComponent::HasWallRunInput(const FVector& RunDirection, const FVector& Normal) const
{
	//壁沿いに進む入力があり、壁から大きく離れる入力ではないこと
	const FVector InputDir = FVector(Acceleration.X, Acceleration.Y, 0.0f).GetSafeNormal();
	return !InputDir.IsNearlyZero()
		&& FVector::DotProduct(InputDir, RunDirection) > 0.1f
		&& FVector::DotProduct(InputDir, Normal) < 0.7f;
}

bool USavaCharacterMovementComponent::TryStartWallRun()
{
	//しゃがみ中(着地スライディング狙い)や、速く落下中は張り付かない
	if (bWantsToCrouch || Velocity.Z < -WallRunMaxEntryFallSpeed)
	{
		return false;
	}

	const FVector Horizontal(Velocity.X, Velocity.Y, 0.0f);
	if (Horizontal.SizeSquared() < FMath::Square(WallRunMinSpeed))
	{
		return false;
	}

	//足元の近くに床があるなら始めない
	const float HalfHeight = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Feet = UpdatedComponent->GetComponentLocation() - FVector(0.0f, 0.0f, HalfHeight);
	FHitResult GroundHit;
	if (TraceMovementLine(Feet, Feet - FVector(0.0f, 0.0f, WallRunMinHeight), GroundHit))
	{
		return false;
	}

	//進行方向の左右に壁を探す
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Horizontal.GetSafeNormal());
	for (const float Sign : { 1.0f, -1.0f })
	{
		FHitResult WallHit;
		if (!FindWallRunWall(Side * Sign, WallHit))
		{
			continue;
		}

		//着地する前に走った壁には、もう一度張り付かない
		const FVector Normal = WallHit.ImpactNormal.GetSafeNormal2D();
		if (!LastWallRunNormal.IsZero() && FVector::DotProduct(Normal, LastWallRunNormal) > 0.9f)
		{
			continue;
		}

		const FVector RunDir = FVector::VectorPlaneProject(Horizontal, Normal).GetSafeNormal();
		if (!HasWallRunInput(RunDir, Normal))
		{
			continue;
		}

		//水平の勢いは壁沿いの向きに保ち、上下の速度は抑える
		WallRunNormal = Normal;
		WallRunElapsed = 0.0f;
		Velocity = RunDir * Horizontal.Size() + FVector(0.0f, 0.0f, FMath::Clamp(Velocity.Z, 0.0f, WallRunMaxEntryUpSpeed));
		SetMovementMode(MOVE_Custom, CMOVE_WallRun);
		return true;
	}
	return false;
}

void USavaCharacterMovementComponent::ExitWallRun()
{
	LastWallRunNormal = WallRunNormal;
	SetMovementMode(MOVE_Falling);
}

bool USavaCharacterMovementComponent::DoJump(bool bReplayingMoves, float DeltaTime)
{
	//壁走り中のジャンプはウォールジャンプ: 壁沿いの勢いを保ったまま、壁から離れる方向と上に飛ぶ
	if (IsWallRunning())
	{
		if (!CharacterOwner || !CharacterOwner->CanJump())
		{
			return false;
		}

		Velocity = FVector(Velocity.X, Velocity.Y, 0.0f) + WallRunNormal * WallJumpOffSpeed;
		Velocity.Z = WallJumpUpSpeed;
		ExitWallRun();
		return true;
	}
	return Super::DoJump(bReplayingMoves, DeltaTime);
}

void USavaCharacterMovementComponent::PhysWallRun(float DeltaTime, int32 Iterations)
{
	if (DeltaTime < MIN_TICK_TIME)
	{
		return;
	}
	WallRunElapsed += DeltaTime;

	//壁がまだ横にあるか確認(壁の向きは毎フレーム更新して、曲がった壁にも沿う)
	FHitResult WallHit;
	const bool bHasWall = FindWallRunWall(-WallRunNormal, WallHit);
	if (bHasWall)
	{
		WallRunNormal = WallHit.ImpactNormal.GetSafeNormal2D();
	}

	const FVector Horizontal(Velocity.X, Velocity.Y, 0.0f);
	const FVector RunDir = FVector::VectorPlaneProject(Horizontal, WallRunNormal).GetSafeNormal();

	//終了: 壁がない / 時間切れ / 壁沿いの入力がない / 遅すぎる
	if (!bHasWall || WallRunElapsed >= WallRunMaxDuration || !HasWallRunInput(RunDir, WallRunNormal)
		|| Horizontal.SizeSquared() < FMath::Square(WallRunMinSpeed))
	{
		ExitWallRun();
		StartNewPhysics(DeltaTime, Iterations);
		return;
	}

	//水平: 目標速度まで加速し、超えている分は空中と同じ割合で減らす
	const float TargetSpeed = MaxWalkSpeed * WallRunSpeedMultiplier * GetAbilityMoveSpeedMultiplier();
	float Speed = Horizontal.Size();
	Speed = Speed < TargetSpeed
		? FMath::Min(Speed + WallRunAcceleration * DeltaTime, TargetSpeed)
		: ApplyOverspeedDecay(Speed, TargetSpeed, DeltaTime);

	//上下: 弱い重力でゆっくり下がる
	const float VerticalSpeed = Velocity.Z + GetGravityZ() * WallRunGravityScale * DeltaTime;
	Velocity = RunDir * Speed + FVector(0.0f, 0.0f, VerticalSpeed);

	//移動(壁に少し押し付けて、離れないようにする)
	Iterations++;
	bJustTeleported = false;
	const FVector OldLocation = UpdatedComponent->GetComponentLocation();
	const FVector Delta = (Velocity - WallRunNormal * WallRunStickSpeed) * DeltaTime;
	FHitResult Hit(1.0f);
	SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);

	if (Hit.Time < 1.0f)
	{
		//床に着いたら着地(着地スライディングの判定もここで行われる)
		if (IsValidLandingSpot(UpdatedComponent->GetComponentLocation(), Hit))
		{
			ProcessLanded(Hit, DeltaTime * (1.0f - Hit.Time), Iterations);
			return;
		}
		HandleImpact(Hit, DeltaTime, Delta);
		SlideAlongSurface(Delta, 1.0f - Hit.Time, Hit.Normal, Hit, true);
	}

	//実際に動いた量から速度を更新(壁への押し付け分は次のフレームで取り除かれる)
	if (!bJustTeleported)
	{
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / DeltaTime;
	}
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
	//通常はしゃがみボタン押下中はジャンプ不可だが、スライディング中は許可する。壁走り中はウォールジャンプ
	if (IsSliding() || IsWallRunning())
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

	if (bCanChangeState)
	{
		SlideBoostCooldownRemaining = FMath::Max(SlideBoostCooldownRemaining - DeltaSeconds, 0.0f);
	}

	//しゃがみボタンを離したらスライディング終了(立ち上がりは下の Super が行う)
	if (bCanChangeState && IsSliding() && !bWantsToCrouch)
	{
		SetMovementMode(MOVE_Walking);
	}

	//壁走り: しゃがみで壁から降りる / 落下中に条件を満たす壁があれば開始
	if (bCanChangeState)
	{
		if (IsWallRunning() && bWantsToCrouch)
		{
			ExitWallRun();
		}
		else if (IsFalling())
		{
			TryStartWallRun();
		}
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

void USavaCharacterMovementComponent::SetPostLandedPhysics(const FHitResult& Hit)
{
	//ここで歩き状態になる(床の取得・縦方向の速度の除去も済む)
	Super::SetPostLandedPhysics(Hit);

	//着地したので、どの壁にもまた張り付ける
	LastWallRunNormal = FVector::ZeroVector;

	//地上のブレーキがかかる前に判定する。しゃがみ(カプセル縮小)がまだなら次のフレームで行われる
	if (bSlideOnLanding && bWantsToCrouch && MovementMode == MOVE_Walking
		&& Velocity.SizeSquared2D() >= FMath::Square(SlideMinStartSpeed))
	{
		EnterSlide();
	}
}

void USavaCharacterMovementComponent::EnterSlide()
{
	//ブーストはクールダウンが終わっているときだけ(連続スライディングで無限に加速しないように)
	if (SlideBoostCooldownRemaining <= 0.0f)
	{
		Velocity += Velocity.GetSafeNormal2D() * SlideEnterImpulse;
		SlideBoostCooldownRemaining = SlideBoostCooldown;
	}
	SetMovementMode(MOVE_Custom, CMOVE_Slide);

	//地面にいるので、しゃがみ/立ち上がりは足元を基準にする(Custom モードでは既定で false になる)
	bCrouchMaintainsBaseLocation = true;
	FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, false);
}

void USavaCharacterMovementComponent::PhysCustom(float DeltaTime, int32 Iterations)
{
	switch (CustomMovementMode)
	{
	case CMOVE_Slide:
		PhysSlide(DeltaTime, Iterations);
		break;
	case CMOVE_WallRun:
		PhysWallRun(DeltaTime, Iterations);
		break;
	default:
		break;
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

void USavaCharacterMovementComponent::ClientHandleMoveResponse(const FCharacterMoveResponseDataContainer& MoveResponse)
{
	//補正時はサーバーの値に合わせる(この後、保存済みの入力が再実行されて現在まで進む)
	if (MoveResponse.IsCorrection())
	{
		const FSavaCharacterMoveResponseDataContainer& SavaResponse = static_cast<const FSavaCharacterMoveResponseDataContainer&>(MoveResponse);
		SlideBoostCooldownRemaining = SavaResponse.SlideBoostCooldownRemaining;
		WallRunNormal = SavaResponse.WallRunNormal;
		WallRunElapsed = SavaResponse.WallRunElapsed;
		LastWallRunNormal = SavaResponse.LastWallRunNormal;
	}
	Super::ClientHandleMoveResponse(MoveResponse);
}

void FSavaCharacterMoveResponseDataContainer::ServerFillResponseData(const UCharacterMovementComponent& CharacterMovement, const FClientAdjustment& PendingAdjustment)
{
	Super::ServerFillResponseData(CharacterMovement, PendingAdjustment);

	//このコンテナは USavaCharacterMovementComponent にしか設定しない
	const USavaCharacterMovementComponent& SavaMovement = static_cast<const USavaCharacterMovementComponent&>(CharacterMovement);
	SlideBoostCooldownRemaining = SavaMovement.SlideBoostCooldownRemaining;
	WallRunNormal = SavaMovement.WallRunNormal;
	WallRunElapsed = SavaMovement.WallRunElapsed;
	LastWallRunNormal = SavaMovement.LastWallRunNormal;
}

bool FSavaCharacterMoveResponseDataContainer::Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap)
{
	if (!Super::Serialize(CharacterMovement, Ar, PackageMap))
	{
		return false;
	}

	//補正のときだけ送る(問題なしの応答では送らない)
	if (IsCorrection())
	{
		Ar << SlideBoostCooldownRemaining;
		Ar << WallRunNormal;
		Ar << WallRunElapsed;
		Ar << LastWallRunNormal;
	}
	return !Ar.IsError();
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
// Fill out your copyright notice in the Description page of Project Settings.


#include "SavaCharacterMovementComponent.h"
#include "AbilitySystem/SavaAttributeSet.h"
#include "AbilitySystemGlobals.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"

USavaCharacterMovementComponent::USavaCharacterMovementComponent()
{
	//しゃがみを有効化(初期値は false で Crouch() が何もしない)
	NavAgentProps.bCanCrouch = true;

	//エンジン標準の移動の値(調整済みの値を初期値にする)
	MaxWalkSpeed = 800.0f;
	MaxAcceleration = 4000.0f;					//約0.2秒で最高速
	BrakingDecelerationWalking = 4000.0f;		//手を離すとすぐ止まる
	AirControl = 0.6f;							//空中でも方向を変えやすく
	BrakingDecelerationFalling = 0.0f;			//空中で減速しない(超過分の減速は AirOverspeedDecayRate)

	//補正時に独自の状態も送る
	SetMoveResponseDataContainer(SavaMoveResponseDataContainer);

	//他のプレイヤーの表示の遅れを減らす(標準は 0.1 秒。高速移動だと遅れが目立つ。短くしすぎるとカクつくので 60Hz の更新間隔に合わせた値)
	NetworkSimulatedSmoothLocationTime = 0.06f;
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
	bJumpHeld = (Flags & FSavedMove_Character::FLAG_Custom_1) != 0;
}

//--------------------------------Blueprint events

TEnumAsByte<ESavaCustomMovementMode> USavaCharacterMovementComponent::GetCustomMovementModeType() const
{
	return MovementMode == MOVE_Custom ? static_cast<ESavaCustomMovementMode>(CustomMovementMode) : CMOVE_None;
}

void USavaCharacterMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

	//ラーチは空中だけ。着地や壁走りに移ったら終わる
	if (MovementMode != MOVE_Falling && LurchTimeRemaining > 0.0f)
	{
		EndLurch();
	}

	const bool bWallJumped = bWallJumpedThisMove;
	bWallJumpedThisMove = false;

	const ESavaCustomMovementMode OldMode = PreviousMovementMode == MOVE_Custom
		? static_cast<ESavaCustomMovementMode>(PreviousCustomMode) : CMOVE_None;
	const ESavaCustomMovementMode NewMode = GetCustomMovementModeType();
	if (OldMode == NewMode)
	{
		return;
	}

	//補正の後に入力を再実行している間は、同じ演出が二重に出るので知らせない
	if (CharacterOwner && CharacterOwner->bClientUpdating)
	{
		return;
	}

	switch (OldMode)
	{
	case CMOVE_Slide:
		OnSlideEnded.Broadcast();
		break;
	case CMOVE_WallRun:
		OnWallRunEnded.Broadcast();
		if (bWallJumped)
		{
			OnWallJumped.Broadcast();
		}
		break;
	case CMOVE_Mantle:
		OnMantleEnded.Broadcast();
		break;
	default:
		break;
	}

	switch (NewMode)
	{
	case CMOVE_Slide:
		OnSlideStarted.Broadcast();
		break;
	case CMOVE_WallRun:
		OnWallRunStarted.Broadcast();
		break;
	case CMOVE_Mantle:
		OnMantleStarted.Broadcast();
		break;
	default:
		break;
	}

	OnCustomMovementModeChanged.Broadcast(OldMode, NewMode);
}

//--------------------------------Air

float USavaCharacterMovementComponent::GetGravityZ() const
{
	//空中で下降しているときだけ重力を強める(ジャンプの上昇・壁走り・スライディングなどはそのまま)
	//速度と移動の種類だけで決まるので、サーバーとクライアントで同じ結果になる
	const float GravityZ = Super::GetGravityZ();
	if (MovementMode == MOVE_Falling && Velocity.Z <= 0.0f)
	{
		return GravityZ * FallingGravityScale;
	}
	return GravityZ;
}

void USavaCharacterMovementComponent::PhysFalling(float DeltaTime, int32 Iterations)
{
	ApplyLurch(DeltaTime);
	ApplyAirOverspeedDecay(DeltaTime);
	Super::PhysFalling(DeltaTime, Iterations);
}

//--------------------------------Lurch

void USavaCharacterMovementComponent::StartLurch()
{
	//ジャンプした瞬間の入力を覚えておく(これと違う向きの入力だけで曲がる)
	LurchTimeRemaining = LurchWindow;
	LurchAngleRemaining = LurchMaxAngle;
	LurchInputDir = FVector(Acceleration.X, Acceleration.Y, 0.0f).GetSafeNormal();
}

void USavaCharacterMovementComponent::ApplyLurch(float DeltaTime)
{
	if (LurchTimeRemaining <= 0.0f)
	{
		return;
	}
	LurchTimeRemaining = FMath::Max(LurchTimeRemaining - DeltaTime, 0.0f);
	TurnTowardLurchInput(DeltaTime);

	//時間切れか、曲がれる角度を使い切ったら終わり
	if (LurchTimeRemaining <= 0.0f || LurchAngleRemaining <= 0.0f)
	{
		EndLurch();
	}
}

void USavaCharacterMovementComponent::EndLurch()
{
	//デバッグ表示: 実際に曲がったときだけ知らせる
	const float TurnedAngle = LurchMaxAngle - LurchAngleRemaining;
	if (TurnedAngle > 0.5f)
	{
		ShowTechniqueDebug(FString::Printf(TEXT("Lurch  %.0f deg"), TurnedAngle), FColor::Cyan);
	}
	LurchTimeRemaining = 0.0f;
	LurchAngleRemaining = 0.0f;
}

void USavaCharacterMovementComponent::TurnTowardLurchInput(float DeltaTime)
{
	if (LurchAngleRemaining <= 0.0f)
	{
		return;
	}

	//ジャンプ前から入れっぱなしの向きでは曲がらない(新しく入れ直した向きだけ)
	const FVector InputDir = FVector(Acceleration.X, Acceleration.Y, 0.0f).GetSafeNormal();
	if (InputDir.IsNearlyZero()
		|| FVector::DotProduct(InputDir, LurchInputDir) > FMath::Cos(FMath::DegreesToRadians(LurchInputChangeAngle)))
	{
		return;
	}

	const FVector Horizontal(Velocity.X, Velocity.Y, 0.0f);
	const float Speed = Horizontal.Size();
	if (Speed < KINDA_SMALL_NUMBER)
	{
		return;
	}

	//入力の向きへ、1フレームで曲がれる分・残りの角度の分だけ回す
	const FVector MoveDir = Horizontal / Speed;
	const float AngleToInput = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(MoveDir, InputDir), -1.0f, 1.0f)));
	const float Turn = FMath::Min3(AngleToInput, LurchAngleRemaining, LurchTurnRate * DeltaTime);
	if (Turn <= 0.0f)
	{
		return;
	}
	const float TurnSign = FVector::CrossProduct(MoveDir, InputDir).Z >= 0.0f ? 1.0f : -1.0f;
	LurchAngleRemaining -= Turn;

	//曲がった角度に比例して速度を失う
	const float NewSpeed = Speed * (1.0f - LurchSpeedLossAtMaxAngle * Turn / FMath::Max(LurchMaxAngle, 1.0f));
	const FVector NewDir = MoveDir.RotateAngleAxis(Turn * TurnSign, FVector::UpVector);
	Velocity.X = NewDir.X * NewSpeed;
	Velocity.Y = NewDir.Y * NewSpeed;
}

void USavaCharacterMovementComponent::ApplyAirOverspeedDecay(float DeltaTime)
{
	//基準速度を超えた分だけを指数的に減らす(基準以下の通常ジャンプには影響しない)
	//スキルで速くなっているときは基準も上げる(ジャンプのたびに減速させられないように)
	const float SoftCap = MaxWalkSpeed * AirSpeedSoftCapMultiplier * GetAbilityMoveSpeedMultiplier();
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

FVector USavaCharacterMovementComponent::GetWallRunDirection() const
{
	if (!IsWallRunning())
	{
		return FVector::ZeroVector;
	}
	return FVector::VectorPlaneProject(FVector(Velocity.X, Velocity.Y, 0.0f), WallRunNormal).GetSafeNormal();
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
		&& FVector::DotProduct(InputDir, Normal) < WallRunDetachInputDot;
}

bool USavaCharacterMovementComponent::IsSameWallAsLast(const FVector& Normal) const
{
	return !LastWallRunNormal.IsZero() && FVector::DotProduct(Normal, LastWallRunNormal) > 0.9f;
}

bool USavaCharacterMovementComponent::TryStartWallRun()
{
	//ジャンプボタンを押している間だけ始められる。地上ジャンプの直後は少しの間始められない
	if (!bJumpHeld || WallRunStartDelayRemaining > 0.0f)
	{
		return false;
	}

	//速く落下中は張り付かない(空中でしゃがみを押していても、着地スライディングより壁走りを優先する)
	if (Velocity.Z < -WallRunMaxEntryFallSpeed)
	{
		return false;
	}

	const FVector Horizontal(Velocity.X, Velocity.Y, 0.0f);
	if (Horizontal.SizeSquared() < FMath::Square(WallRunMinSpeed))
	{
		return false;
	}

	//落下中に足元の近くに床があるなら始めない(着地寸前に張り付かないように)
	//上昇中はジャンプした直後でも張り付ける
	if (Velocity.Z <= 0.0f)
	{
		const float HalfHeight = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const FVector Feet = UpdatedComponent->GetComponentLocation() - FVector(0.0f, 0.0f, HalfHeight);
		FHitResult GroundHit;
		if (TraceMovementLine(Feet, Feet - FVector(0.0f, 0.0f, WallRunMinHeight), GroundHit))
		{
			return false;
		}
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

		//直前と同じ壁: 離れた直後(クールダウン中)と、この壁での壁走りの時間を使い切った後は張り付かない
		const FVector Normal = WallHit.ImpactNormal.GetSafeNormal2D();
		const bool bSameWall = IsSameWallAsLast(Normal);
		if (bSameWall && (WallRunSameWallCooldownRemaining > 0.0f || WallRunElapsed >= WallRunMaxDuration))
		{
			continue;
		}

		const FVector RunDir = FVector::VectorPlaneProject(Horizontal, Normal).GetSafeNormal();
		if (!HasWallRunInput(RunDir, Normal))
		{
			continue;
		}

		//後ろ向き(進行方向と逆を見ている)では張り付かない
		const FVector Facing = UpdatedComponent->GetForwardVector().GetSafeNormal2D();
		if (FVector::DotProduct(Facing, RunDir) < FMath::Cos(FMath::DegreesToRadians(WallRunMaxEntryLookAngle)))
		{
			continue;
		}

		//水平の勢いは壁沿いの向きに保つ。上下の勢いも残す(上昇しすぎだけ抑える)
		WallRunNormal = Normal;
		//同じ壁なら壁走りの時間を引き継ぐ(張り付き直しで時間の上限をリセットさせない)
		if (!bSameWall)
		{
			WallRunElapsed = 0.0f;
		}
		bWallRunOnSameWall = bSameWall;
		WallTapElapsed = 0.0f;
		WallJumpLateReleaseRemaining = 0.0f;
		const float EntryVerticalSpeed = FMath::Min(Velocity.Z * WallRunEntryVerticalScale, WallRunMaxEntryUpSpeed);
		Velocity = RunDir * Horizontal.Size() + FVector(0.0f, 0.0f, EntryVerticalSpeed);
		SetMovementMode(MOVE_Custom, CMOVE_WallRun);
		return true;
	}
	return false;
}

void USavaCharacterMovementComponent::ExitWallRun()
{
	//壁走りの共通の終わり方
	LastWallRunNormal = WallRunNormal;
	WallRunSameWallCooldownRemaining = WallRunSameWallCooldown;
	SetMovementMode(MOVE_Falling);
}

float USavaCharacterMovementComponent::GetNextWallJumpUpSpeed() const
{
	//繰り返すほど下限(水平より少し下)へ近づく
	const float Alpha = FMath::Clamp(WallJumpDecay / FMath::Max(WallJumpAngleDecaySteps, 1.0f), 0.0f, 1.0f);
	return FMath::Lerp(WallJumpUpSpeed, WallJumpMinUpSpeed, Alpha);
}

void USavaCharacterMovementComponent::DoWallJump()
{
	//ジャンプボタンを離したときに呼ぶ: 壁沿いの勢いを保ったまま、壁から離れる方向と上に飛ぶ

	//ウォールタップ: 張り付いてすぐ跳ぶほど(= すぐ離すほど)、壁沿いの速度が上乗せされる
	//普通のウォールジャンプは壁沿いの速度を少し失う。ウォールタップが早いほど失う量が減る
	//同じ壁に張り付き直したときは上乗せなし(1枚の壁で繰り返して無限に加速しないように)。減速しない効果は残す
	float TapAlpha = 0.0f;
	FVector TapBonus = FVector::ZeroVector;
	if (IsWallRunning() && WallTapWindow > 0.0f)
	{
		TapAlpha = 1.0f - FMath::Clamp(WallTapElapsed / WallTapWindow, 0.0f, 1.0f);
		if (!bWallRunOnSameWall)
		{
			TapBonus = GetWallRunDirection() * WallTapBonusSpeed * TapAlpha;
		}
	}
	const float SpeedLoss = WallJumpSpeedLoss * (1.0f - TapAlpha);
	const FVector AlongWall = FVector(Velocity.X, Velocity.Y, 0.0f) * (1.0f - SpeedLoss);

	//上向きの速度: 繰り返すほど下がる(同じ壁で繰り返すと WallJumpSameWallDecayScale 倍の速さで下がる)
	const float UpSpeed = GetNextWallJumpUpSpeed();
	WallJumpDecay += bWallRunOnSameWall ? WallJumpSameWallDecayScale : 1.0f;

	//壁走りが途切れた直後(猶予中)のウォールジャンプか
	const bool bLateRelease = !IsWallRunning();

	const FString WallInfo = FString::Printf(TEXT("up %+.0f%s"), UpSpeed, bWallRunOnSameWall ? TEXT(", same wall") : TEXT(""));
	if (bLateRelease)
	{
		ShowTechniqueDebug(FString::Printf(TEXT("Wall Jump (late release)  -%.0f  (%s)"),
			Velocity.Size2D() - AlongWall.Size(), *WallInfo), FColor::Silver);
	}
	else if (TapAlpha > 0.0f)
	{
		ShowTechniqueDebug(FString::Printf(TEXT("Wall Tap  +%.0f  loss %.0f%%  (%.2fs after contact, %s)"),
			TapBonus.Size(), SpeedLoss * 100.0f, WallTapElapsed, *WallInfo), FColor::Green);
	}
	else
	{
		ShowTechniqueDebug(FString::Printf(TEXT("Wall Jump  -%.0f  (tap missed: %.2fs after contact, %s)"),
			Velocity.Size2D() - AlongWall.Size(), WallTapElapsed, *WallInfo), FColor::Silver);
	}

	Velocity = AlongWall + TapBonus + WallRunNormal * WallJumpOffSpeed;
	Velocity.Z = UpSpeed;
	WallJumpLateReleaseRemaining = 0.0f;
	if (bLateRelease)
	{
		//すでに落下中で移動の種類が変わらないので、ここで知らせる(補正後のやり直し中は二重になるので知らせない)
		WallRunSameWallCooldownRemaining = WallRunSameWallCooldown;
		if (!CharacterOwner->bClientUpdating)
		{
			OnWallJumped.Broadcast();
		}
	}
	else
	{
		bWallJumpedThisMove = true;
		ExitWallRun();
	}
	StartLurch();
}

bool USavaCharacterMovementComponent::DoJump(bool bReplayingMoves, float DeltaTime)
{
	//壁走り中はジャンプボタンを押しても何もしない(ウォールジャンプはボタンを離したときに DoWallJump で行う)
	if (IsWallRunning())
	{
		return false;
	}

	if (IsFalling())
	{
		return DoAirJump(bReplayingMoves, DeltaTime);
	}

	//地上のジャンプ: 跳ぶ前の状態でクレスト / エッジを判定する(両方満たすならクレストだけ)
	const bool bWasOnGround = IsMovingOnGround();
	const bool bWasSliding = IsSliding();
	FString CrestFailReason;
	const bool bCrestJump = bWasOnGround && IsCrestJumpAvailable(&CrestFailReason);
	const bool bEdgeJump = bWasOnGround && !bCrestJump && IsEdgeJumpAvailable(EdgeJumpWindow);
	//デバッグ表示用: 端の少し手前で跳んだ(早すぎた)か
	const bool bEdgeTooEarly = bWasOnGround && !bCrestJump && !bEdgeJump && IsEdgeJumpAvailable(EdgeJumpWindow * 3.0f);

	if (!Super::DoJump(bReplayingMoves, DeltaTime))
	{
		return false;
	}

	//跳んだ直後は、ボタンを押したままでも少しの間は壁走りを始めない
	WallRunStartDelayRemaining = WallRunAfterGroundJumpDelay;

	if (bCrestJump)
	{
		//坂を上っていた上向きの勢い(頂上を越えた直後なら、越える前の値)を、通常のジャンプに足す
		const float CrestBonus = FMath::Clamp(CrestUpSpeed * CrestJumpUpSpeedScale, 0.0f, CrestJumpMaxBonusUpSpeed);
		Velocity.Z = JumpZVelocity + CrestBonus;
		CrestTimeRemaining = 0.0f;
		ShowTechniqueDebug(FString::Printf(TEXT("Crest Jump  +%.0f up"), CrestBonus), FColor::Green);
	}
	else if (bEdgeJump)
	{
		//減速しない上に、少し上乗せされる
		Velocity += Velocity.GetSafeNormal2D() * EdgeJumpBonusSpeed;
		ShowTechniqueDebug(FString::Printf(TEXT("Edge Jump  +%.0f  (no loss)"), EdgeJumpBonusSpeed), FColor::Green);
	}
	else
	{
		//普通のジャンプは水平の速度を少し失う(スライディングを始められる速度より下には減らさない。
		//スライディングできる速さで跳べば、着地スライディングも必ずできる)
		const float Speed = Velocity.Size2D();
		const float SlideStartSpeed = MaxWalkSpeed * GetAbilityMoveSpeedMultiplier() * SlideMinStartWalkSpeedRate;
		const float NewSpeed = FMath::Max(Speed * (1.0f - JumpSpeedLoss), FMath::Min(Speed, SlideStartSpeed));
		if (Speed > KINDA_SMALL_NUMBER)
		{
			Velocity.X *= NewSpeed / Speed;
			Velocity.Y *= NewSpeed / Speed;
		}
		const FString JumpName = bWasSliding ? FString::Printf(TEXT("Slide Jump  (crest failed: %s)"), *CrestFailReason) : FString(TEXT("Jump"));
		ShowTechniqueDebug(FString::Printf(TEXT("%s  -%.0f%s"), *JumpName, Speed - NewSpeed,
			bEdgeTooEarly ? TEXT("  (edge: too early)") : TEXT("")), FColor::Silver);
	}
	StartLurch();
	return true;
}

//--------------------------------DoubleJump

bool USavaCharacterMovementComponent::DoAirJump(bool bReplayingMoves, float DeltaTime)
{
	//ボタン長押しで続けて呼ばれている間(JumpMaxHoldTime を使う設定)は標準の処理のまま。二段ジャンプは押し直したときだけ
	if (CharacterOwner->bWasJumping)
	{
		return Super::DoJump(bReplayingMoves, DeltaTime);
	}

	//エンジンは跳べないときもこの関数を呼ぶので、回数が残っているかはここで確かめる
	if (!CharacterOwner->CanJump())
	{
		return false;
	}

	//壁走り・よじ登りを始められるなら、そちらを優先して二段ジャンプは使わない(壁走りに入ると回数も戻る)
	if (TryStartMantle() || TryStartWallRun())
	{
		return false;
	}

	DoDoubleJump();
	return true;
}

void USavaCharacterMovementComponent::DoDoubleJump()
{
	//上昇が速いほど、上ではなく横へ飛ぶ(DoubleJumpSidewaysRiseSpeed 以上で上には足さない。今の上昇の勢いは残す)
	//ジャンプ直後ほど上昇が速いので、ジャンプしてから早く押すほど水平のブーストが大きくなる
	const float RiseAlpha = DoubleJumpSidewaysRiseSpeed > 0.0f
		? FMath::Clamp(Velocity.Z / DoubleJumpSidewaysRiseSpeed, 0.0f, 1.0f) : 0.0f;
	const float UpSpeed = FMath::Max(Velocity.Z, DoubleJumpZVelocity * (1.0f - RiseAlpha));
	const float SidewaysSpeed = DoubleJumpSidewaysSpeed * RiseAlpha;

	//入力があれば、その向きへ進行方向を変える(大きく変えるほど速度を失う)
	const FVector Horizontal(Velocity.X, Velocity.Y, 0.0f);
	float Speed = Horizontal.Size();
	FVector MoveDir = Speed > KINDA_SMALL_NUMBER ? Horizontal / Speed : FVector::ZeroVector;
	const FVector InputDir = FVector(Acceleration.X, Acceleration.Y, 0.0f).GetSafeNormal();
	float TurnAngle = 0.0f;
	if (!InputDir.IsNearlyZero())
	{
		if (!MoveDir.IsZero())
		{
			TurnAngle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(MoveDir, InputDir), -1.0f, 1.0f)));
			Speed *= 1.0f - DoubleJumpRedirectSpeedLoss * TurnAngle / 180.0f;
		}
		MoveDir = InputDir;
	}

	//入力も水平の動きもなければ、真上に跳ぶだけ(横へのブーストは出ない)
	const float NewSpeed = MoveDir.IsZero() ? 0.0f : Speed + SidewaysSpeed;
	ShowTechniqueDebug(FString::Printf(TEXT("Double Jump  up %+.0f  side +%.0f (rise %.0f)  turn %.0f deg"),
		UpSpeed - Velocity.Z, SidewaysSpeed, Velocity.Z, TurnAngle),
		SidewaysSpeed > 0.0f ? FColor::Green : FColor::Silver);

	Velocity = MoveDir * NewSpeed + FVector(0.0f, 0.0f, UpSpeed);

	//向きはここで変えたので、前のジャンプのラーチは終わらせる
	EndLurch();

	//補正後のやり直し中は二重になるので知らせない
	if (!CharacterOwner->bClientUpdating)
	{
		OnDoubleJumped.Broadcast();
	}
}

//--------------------------------EdgeJump / CrestJump

bool USavaCharacterMovementComponent::IsEdgeJumpAvailable(float LookAheadTime) const
{
	const float Speed = Velocity.Size2D();
	if (Speed < EdgeJumpMinSpeed)
	{
		return false;
	}

	//LookAheadTime 秒後にいる位置(途中に壁があれば端ではない)
	const FVector MoveDir = Velocity.GetSafeNormal2D();
	const FVector Location = UpdatedComponent->GetComponentLocation();
	const FVector Ahead = Location + MoveDir * Speed * LookAheadTime;
	if (!IsCapsuleSweepClear(Location, Ahead))
	{
		return false;
	}

	//その位置から体(カプセル)ごと下へ動かして、EdgeJumpMinDropHeight 以内に何もなければ端
	//(体の一部でも足場に残っている間は落ちないので、中心ではなく体全体で調べる)
	return IsCapsuleSweepClear(Ahead, Ahead - FVector(0.0f, 0.0f, EdgeJumpMinDropHeight));
}

float USavaCharacterMovementComponent::GetSlopeTanAlongVelocity() const
{
	//足元の床の傾き(進む向きで見た勾配。上りならプラス、1.0 = 45度)
	const FVector MoveDir = Velocity.GetSafeNormal2D();
	const FVector FloorNormal = CurrentFloor.HitResult.ImpactNormal;
	if (!CurrentFloor.IsWalkableFloor() || MoveDir.IsNearlyZero() || FloorNormal.Z < KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}
	return -FVector::DotProduct(FloorNormal, MoveDir) / FloorNormal.Z;
}

bool USavaCharacterMovementComponent::IsCrestJumpAvailable(FString* OutFailReason) const
{
	if (!IsSliding())
	{
		if (OutFailReason) { *OutFailReason = TEXT("not sliding"); }
		return false;
	}

	//上り坂を進んでいなければ、頂上を越えた直後(受付時間内)だけ成立
	const float SlopeTan = GetSlopeTanAlongVelocity();
	if (SlopeTan < FMath::Tan(FMath::DegreesToRadians(CrestJumpMinSlopeAngle)))
	{
		if (CrestTimeRemaining > 0.0f)
		{
			return true;
		}
		if (OutFailReason)
		{
			*OutFailReason = FString::Printf(TEXT("slope %.0f deg < %.0f deg"), FMath::RadiansToDegrees(FMath::Atan(SlopeTan)), CrestJumpMinSlopeAngle);
		}
		return false;
	}

	//上り坂の途中: 今の坂がそのまま続いた場合の、少し先の床の高さ
	const FVector MoveDir = Velocity.GetSafeNormal2D();
	const FVector FloorNormal = CurrentFloor.HitResult.ImpactNormal;
	const FVector FloorPoint = CurrentFloor.HitResult.ImpactPoint;
	const FVector AheadXY = UpdatedComponent->GetComponentLocation() + MoveDir * CrestJumpCheckDistance;
	const float ExpectedZ = FloorPoint.Z
		- FVector::DotProduct(FVector(AheadXY.X - FloorPoint.X, AheadXY.Y - FloorPoint.Y, 0.0f), FloorNormal) / FloorNormal.Z;

	//その高さ付近から上に床がなければ、坂はこの先で終わっている(= 頂上)
	//(上から調べるので、坂がさらに急になる場所は頂上と間違えない)
	const FVector TraceStart(AheadXY.X, AheadXY.Y, ExpectedZ + CrestJumpCheckDistance);
	const FVector TraceEnd(AheadXY.X, AheadXY.Y, ExpectedZ - CrestJumpHeightTolerance);
	FHitResult SlopeHit;
	if (TraceMovementLine(TraceStart, TraceEnd, SlopeHit))
	{
		if (OutFailReason)
		{
			*OutFailReason = FString::Printf(TEXT("climbing, but top is not within %.0fcm"), CrestJumpCheckDistance);
		}
		return false;
	}
	return true;
}

void USavaCharacterMovementComponent::PhysWallRun(float DeltaTime, int32 Iterations)
{
	if (DeltaTime < MIN_TICK_TIME)
	{
		return;
	}
	WallRunElapsed += DeltaTime;
	WallTapElapsed += DeltaTime;

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
	//(この後少しの間は、ジャンプを離せばウォールジャンプになる)
	if (!bHasWall || WallRunElapsed >= WallRunMaxDuration || !HasWallRunInput(RunDir, WallRunNormal)
		|| Horizontal.SizeSquared() < FMath::Square(WallRunMinSpeed))
	{
		WallJumpLateReleaseRemaining = WallJumpLateReleaseTime;
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

	//上下: 上昇中は上昇用の重力(通常と同じなら張り付いても登り続けない)、下降中は弱い重力でゆっくり下がる
	const float WallRunGravity = Velocity.Z > 0.0f ? WallRunRisingGravityScale : WallRunGravityScale;
	const float VerticalSpeed = Velocity.Z + GetGravityZ() * WallRunGravity * DeltaTime;
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
	//上向きは計算した値を超えないようにする(斜めの障害物に沿って押し上げられた分で、上へ打ち上げられないように)
	if (!bJustTeleported)
	{
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / DeltaTime;
		Velocity.Z = FMath::Min(Velocity.Z, FMath::Max(VerticalSpeed, 0.0f));
	}
}

//--------------------------------Mantle

bool USavaCharacterMovementComponent::IsMantling() const
{
	return MovementMode == MOVE_Custom && CustomMovementMode == CMOVE_Mantle;
}

bool USavaCharacterMovementComponent::IsCapsuleSweepClear(const FVector& Start, const FVector& End) const
{
	//今のカプセルの形で、Start から End まで何にも当たらずに動けるか
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SavaMantle), false, CharacterOwner);
	FCollisionResponseParams ResponseParams;
	InitCollisionParams(Params, ResponseParams);
	const FCollisionShape Shape = CharacterOwner->GetCapsuleComponent()->GetCollisionShape();
	FHitResult Hit;
	return !GetWorld()->SweepSingleByChannel(Hit, Start, End, UpdatedComponent->GetComponentQuat(),
		UpdatedComponent->GetCollisionObjectType(), Shape, Params, ResponseParams);
}

bool USavaCharacterMovementComponent::TryStartMantle()
{
	//前進の入力がなければ登らない
	const FVector InputDir = FVector(Acceleration.X, Acceleration.Y, 0.0f).GetSafeNormal();
	if (InputDir.IsNearlyZero())
	{
		return false;
	}

	const UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FVector Location = UpdatedComponent->GetComponentLocation();
	const FVector Feet = Location - FVector(0.0f, 0.0f, HalfHeight);
	const FVector Facing = UpdatedComponent->GetForwardVector().GetSafeNormal2D();

	//1. 正面の壁: よじ登る高さの下限から前へ線を飛ばす
	const FVector WallTraceStart = Feet + FVector(0.0f, 0.0f, MantleMinHeight);
	FHitResult WallHit;
	if (!TraceMovementLine(WallTraceStart, WallTraceStart + Facing * (Radius + MantleCheckDistance), WallHit)
		|| !IsValidWallRunWall(WallHit))
	{
		return false;
	}

	//入力と視線が壁の方を向いていること
	const FVector TowardWall = -WallHit.ImpactNormal.GetSafeNormal2D();
	const float MinDot = FMath::Cos(FMath::DegreesToRadians(MantleMaxAngle));
	if (FVector::DotProduct(InputDir, TowardWall) < MinDot || FVector::DotProduct(Facing, TowardWall) < MinDot)
	{
		return false;
	}

	//2. 縁の上面: 壁の少し奥の、上限の高さから下へ線を飛ばす
	const FVector TopTraceStart = FVector(WallHit.ImpactPoint.X, WallHit.ImpactPoint.Y, Feet.Z + MantleMaxHeight) + TowardWall * Radius;
	const FVector TopTraceEnd = FVector(TopTraceStart.X, TopTraceStart.Y, Feet.Z + MantleMinHeight);
	FHitResult TopHit;
	if (!TraceMovementLine(TopTraceStart, TopTraceEnd, TopHit) || TopHit.bStartPenetrating || !IsWalkable(TopHit))
	{
		return false;
	}

	//3. 登った先にカプセルが入り、上→前の経路がふさがっていないこと
	const FVector Target = TopHit.ImpactPoint + FVector(0.0f, 0.0f, HalfHeight + 2.0f);
	const FVector Above = FVector(Location.X, Location.Y, Target.Z);
	if (!IsCapsuleSweepClear(Location, Above) || !IsCapsuleSweepClear(Above, Target))
	{
		return false;
	}

	//高い縁ほど時間をかける
	const float LedgeHeight = TopHit.ImpactPoint.Z - Feet.Z;
	MantleDuration = FMath::GetMappedRangeValueClamped(FVector2D(MantleMinHeight, MantleMaxHeight),
		FVector2D(MantleMinDuration, MantleMaxDuration), LedgeHeight);
	MantleElapsed = 0.0f;
	MantleStartLocation = Location;
	MantleTargetLocation = Target;
	MantleExitDirection = TowardWall;
	Velocity = FVector::ZeroVector;
	SetMovementMode(MOVE_Custom, CMOVE_Mantle);
	ShowTechniqueDebug(FString::Printf(TEXT("Mantle  (ledge %.0fcm)"), LedgeHeight), FColor::White);
	return true;
}

void USavaCharacterMovementComponent::PhysMantle(float DeltaTime, int32 Iterations)
{
	if (DeltaTime < MIN_TICK_TIME)
	{
		return;
	}

	MantleElapsed = FMath::Min(MantleElapsed + DeltaTime, MantleDuration);
	const float Alpha = MantleDuration > 0.0f ? MantleElapsed / MantleDuration : 1.0f;

	//先に上がり切ってから前へ進む(縁の角に引っかからないように)
	constexpr float VerticalPortion = 0.6f;
	const float UpAlpha = FMath::InterpEaseOut(0.0f, 1.0f, FMath::Clamp(Alpha / VerticalPortion, 0.0f, 1.0f), 2.0f);
	const float ForwardAlpha = FMath::Clamp((Alpha - VerticalPortion) / (1.0f - VerticalPortion), 0.0f, 1.0f);

	FVector NewLocation = FMath::Lerp(MantleStartLocation, MantleTargetLocation, ForwardAlpha);
	NewLocation.Z = FMath::Lerp(MantleStartLocation.Z, MantleTargetLocation.Z, UpAlpha);

	Iterations++;
	bJustTeleported = false;
	const FVector OldLocation = UpdatedComponent->GetComponentLocation();
	FHitResult Hit(1.0f);
	SafeMoveUpdatedComponent(NewLocation - OldLocation, UpdatedComponent->GetComponentQuat(), true, Hit);
	Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / DeltaTime;

	//登り終えたら、縁の上を前へ少し進みながら歩きに戻る
	if (Alpha >= 1.0f)
	{
		Velocity = MantleExitDirection * MaxWalkSpeed * MantleExitSpeedMultiplier;
		SetMovementMode(MOVE_Walking);
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
	//通常はしゃがみボタン押下中はジャンプ不可だが、スライディング中と空中(二段ジャンプ)は許可する
	//(壁走り中はジャンプボタンを押しても跳ばない。ウォールジャンプはボタンを離したときに行う)
	if (IsSliding() || IsFalling())
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

	//しゃがみボタンを「押した瞬間」か(押しっぱなしとは区別する)
	bool bCrouchPressed = false;
	if (bCanChangeState)
	{
		SlideBoostCooldownRemaining = FMath::Max(SlideBoostCooldownRemaining - DeltaSeconds, 0.0f);
		CrestTimeRemaining = FMath::Max(CrestTimeRemaining - DeltaSeconds, 0.0f);
		WallRunSameWallCooldownRemaining = FMath::Max(WallRunSameWallCooldownRemaining - DeltaSeconds, 0.0f);
		WallRunStartDelayRemaining = FMath::Max(WallRunStartDelayRemaining - DeltaSeconds, 0.0f);
		WallJumpLateReleaseRemaining = FMath::Max(WallJumpLateReleaseRemaining - DeltaSeconds, 0.0f);
		JustLandingInputRemaining = FMath::Max(JustLandingInputRemaining - DeltaSeconds, 0.0f);
		JustLandingCooldownRemaining = FMath::Max(JustLandingCooldownRemaining - DeltaSeconds, 0.0f);

		bCrouchPressed = bWantsToCrouch && !bPrevWantsToCrouch;
		bPrevWantsToCrouch = bWantsToCrouch;

		//空中でしゃがみを押した瞬間から、ジャストランディングの受付が始まる(押しっぱなしで着地しても成功にはならない)
		//壁走り中に押しても受け付ける(壁走りのまま着地したとき用)
		if (bCrouchPressed && (IsFalling() || IsWallRunning()))
		{
			JustLandingInputRemaining = FMath::Max(JustLandingWindow, KINDA_SMALL_NUMBER);
		}
	}

	//しゃがみボタンを離したらスライディング終了(立ち上がりは下の Super が行う)
	if (bCanChangeState && IsSliding() && !bWantsToCrouch)
	{
		SetMovementMode(MOVE_Walking);
	}

	//壁走り: ジャンプを離したらウォールジャンプ。落下中はジャンプを押している間、条件を満たす壁があれば開始
	//(壁走り中はしゃがみ姿勢が自動で解除される)
	if (bCanChangeState)
	{
		//壁走り中、または壁走りが途切れた直後(猶予中)にジャンプを離したらウォールジャンプ
		if (!bJumpHeld && (IsWallRunning() || (IsFalling() && WallJumpLateReleaseRemaining > 0.0f)))
		{
			DoWallJump();
		}
		else if (IsFalling())
		{
			//正面によじ登れる縁があれば優先する(壁走りは横の壁、よじ登りは正面の壁)
			if (!TryStartMantle())
			{
				TryStartWallRun();
			}
		}
	}

	//しゃがみ / 立ち上がり(よじ登り中はカプセルの大きさを変えない。到着位置は開始時の大きさで確認しているため)
	if (!IsMantling())
	{
		Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
	}

	//このフレームでしゃがんだ + 十分な速度 → スライディング開始
	if (bCanChangeState && !bWasCrouching && IsCrouching() && MovementMode == MOVE_Walking
		&& HasSlideStartSpeed())
	{
		EnterSlide();
	}
}

void USavaCharacterMovementComponent::SetPostLandedPhysics(const FHitResult& Hit)
{
	//ここで歩き状態になる(床の取得・縦方向の速度の除去も済む)
	Super::SetPostLandedPhysics(Hit);

	//着地したので、どの壁でも新しく壁走りできる。ウォールジャンプの角度も元に戻す
	LastWallRunNormal = FVector::ZeroVector;
	WallJumpDecay = 0.0f;
	WallJumpLateReleaseRemaining = 0.0f;

	//空中でしゃがみを押してすぐの着地か(ジャストランディング)
	const bool bJustLanding = JustLandingInputRemaining > 0.0f && bWantsToCrouch;
	JustLandingInputRemaining = 0.0f;

	//地上のブレーキがかかる前に判定する。しゃがみ(カプセル縮小)がまだなら次のフレームで行われる
	if (bSlideOnLanding && bWantsToCrouch && MovementMode == MOVE_Walking && HasSlideStartSpeed())
	{
		EnterSlide(bJustLanding);
	}
}

void USavaCharacterMovementComponent::UpdateCharacterStateAfterMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateAfterMovement(DeltaSeconds);

	//絶対上限速度: どの移動・テクニックの組み合わせでも、水平の速度はこれを超えない(上下の速度はそのまま)
	const float Speed = Velocity.Size2D();
	if (AbsoluteMaxHorizontalSpeed > 0.0f && Speed > AbsoluteMaxHorizontalSpeed)
	{
		const float Scale = AbsoluteMaxHorizontalSpeed / Speed;
		Velocity.X *= Scale;
		Velocity.Y *= Scale;
	}
}

bool USavaCharacterMovementComponent::HasSlideStartSpeed() const
{
	//歩きより速いこと(スキルで速くなっているときは、その分も基準に含める)
	const float MinSpeed = MaxWalkSpeed * GetAbilityMoveSpeedMultiplier() * SlideMinStartWalkSpeedRate;
	return Velocity.SizeSquared2D() >= FMath::Square(MinSpeed);
}

void USavaCharacterMovementComponent::EnterSlide(bool bJustLanding)
{
	//ブーストはクールダウンが終わっているときだけ(連続スライディングで無限に加速しないように)
	//ジャストランディングは別枠のブーストで、使えるならスライディングのブーストより優先する(スライディングのブーストは温存)
	//1回のスライディングで出るブーストは1つだけ
	if (bJustLanding && JustLandingCooldownRemaining <= 0.0f)
	{
		Velocity += Velocity.GetSafeNormal2D() * JustLandingBoostSpeed;
		JustLandingCooldownRemaining = JustLandingCooldown;
		ShowTechniqueDebug(FString::Printf(TEXT("Just Landing  +%.0f  (slide boost kept)"), JustLandingBoostSpeed), FColor::Green);
	}
	else if (SlideBoostCooldownRemaining <= 0.0f)
	{
		Velocity += Velocity.GetSafeNormal2D() * SlideEnterImpulse;
		SlideBoostCooldownRemaining = SlideBoostCooldown;
		if (bJustLanding)
		{
			ShowTechniqueDebug(FString::Printf(TEXT("Just Landing  (cooldown %.1fs, slide boost used)"), JustLandingCooldownRemaining), FColor::White);
		}
	}
	else if (bJustLanding)
	{
		ShowTechniqueDebug(FString::Printf(TEXT("Just Landing  (cooldown %.1fs, slide boost cooldown %.1fs)"),
			JustLandingCooldownRemaining, SlideBoostCooldownRemaining), FColor::Silver);
	}
	SetMovementMode(MOVE_Custom, CMOVE_Slide);

	//地面にいるので、しゃがみ/立ち上がりは足元を基準にする(Custom モードでは既定で false になる)
	bCrouchMaintainsBaseLocation = true;
	FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, false);
}

void USavaCharacterMovementComponent::PhysCustom(float DeltaTime, int32 Iterations)
{
	//歩き・落下は標準の処理が細かく刻むが、独自の移動はそのままだと 1 フレーム分を一度に計算するため、
	//フレームレートが低い側(PIE のクライアントなど)だけ結果が変わってしまう。一定の刻みに揃える
	float RemainingTime = DeltaTime;
	while (RemainingTime >= MIN_TICK_TIME)
	{
		const float Step = FMath::Min(RemainingTime, CustomPhysicsMaxStep);
		RemainingTime -= Step;

		const EMovementMode ModeBefore = MovementMode;
		const uint8 CustomModeBefore = CustomMovementMode;
		switch (CustomMovementMode)
		{
		case CMOVE_Slide:
			PhysSlide(Step, Iterations);
			break;
		case CMOVE_WallRun:
			PhysWallRun(Step, Iterations);
			break;
		case CMOVE_Mantle:
			PhysMantle(Step, Iterations);
			break;
		default:
			break;
		}

		//移動の種類が変わった(着地・落下・終了など)ら、残りの時間は新しい移動で続ける
		if (!HasValidData() || MovementMode != ModeBefore || CustomMovementMode != CustomModeBefore)
		{
			if (HasValidData() && RemainingTime >= MIN_TICK_TIME)
			{
				StartNewPhysics(RemainingTime, Iterations);
			}
			return;
		}
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

	//進む向きで見た下り坂の角度(下りならプラス)。急なほど摩擦を弱める(0 〜 1)
	const FVector MoveDir2D = Velocity.GetSafeNormal2D();
	const float DownhillAngle = FloorNormal.Z > KINDA_SMALL_NUMBER
		? FMath::RadiansToDegrees(FMath::Atan(FVector::DotProduct(FloorNormal, MoveDir2D) / FloorNormal.Z)) : 0.0f;
	const float DownhillAlpha = SlideDownhillFullAngle > 0.0f ? FMath::Clamp(DownhillAngle / SlideDownhillFullAngle, 0.0f, 1.0f) : 0.0f;

	//坂道: 重力の斜面方向の成分で加速(下り) / 減速(上り)。下り坂では重力を強める
	const FVector Gravity(0.0f, 0.0f, GetGravityZ());
	const float SlopeGravityScale = DownhillAngle > 0.0f ? SlideDownhillGravityScale : SlideGravityScale;
	Velocity += FVector::VectorPlaneProject(Gravity, FloorNormal) * SlopeGravityScale * DeltaTime;

	//左右入力で進行方向を少し曲げる(速さは変えない)
	const FVector SteerDir = FVector::VectorPlaneProject(Acceleration, Velocity.GetSafeNormal()).GetSafeNormal2D();
	if (!SteerDir.IsNearlyZero())
	{
		const float CurrentSpeed = Velocity.Size();
		Velocity = (Velocity + SteerDir * SlideSteerAcceleration * DeltaTime).GetSafeNormal() * CurrentSpeed;
	}

	//減速: 速度比例の摩擦 + 一定の制動(下り坂では急なほど弱める)
	const float FrictionScale = FMath::Lerp(1.0f, SlideDownhillFrictionScale, DownhillAlpha);
	const float Speed = Velocity.Size();
	const float NewSpeed = FMath::Max(Speed - (SlideFriction * Speed + SlideBrakingDeceleration) * FrictionScale * DeltaTime, 0.0f);
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
	//上下の速度は、動いた量ではなく床の傾きから決める。段差を乗り越えた分や床への吸着の分を含めると、
	//直後に落下やジャンプへ移ったときにその分だけ上へ打ち上げられてしまう
	if (!bJustTeleported)
	{
		const FVector Moved = (UpdatedComponent->GetComponentLocation() - OldLocation) / DeltaTime;
		Velocity = FVector(Moved.X, Moved.Y, 0.0f);
		const FVector SlopeNormal = CurrentFloor.HitResult.ImpactNormal;
		if (CurrentFloor.IsWalkableFloor() && SlopeNormal.Z > KINDA_SMALL_NUMBER)
		{
			//水平の速度のまま床の面に沿わせたときの上下の速度(上り坂ならプラス)
			Velocity.Z = -(Velocity.X * SlopeNormal.X + Velocity.Y * SlopeNormal.Y) / SlopeNormal.Z;
		}
	}

	//クレストジャンプ用: 上り坂の間は上向きの速度を覚え続け、坂が終わったら受付時間が減り始める
	if (GetSlopeTanAlongVelocity() >= FMath::Tan(FMath::DegreesToRadians(CrestJumpMinSlopeAngle)))
	{
		CrestUpSpeed = FMath::Max(Velocity.Z, 0.0f);
		CrestTimeRemaining = CrestJumpGraceTime;
	}

	if (!CurrentFloor.IsWalkableFloor())
	{
		SetMovementMode(MOVE_Falling);
	}
}

//--------------------------------Debug(一時的なテクニック確認用の表示)

void USavaCharacterMovementComponent::ShowTechniqueDebug(const FString& Text, const FColor& Color) const
{
#if !UE_BUILD_SHIPPING
	//自分が操作しているキャラだけ。補正後の入力のやり直し中は同じ表示が二重に出るので出さない
	if (!bShowTechniqueDebug || !GEngine || !CharacterOwner || !CharacterOwner->IsLocallyControlled() || CharacterOwner->bClientUpdating)
	{
		return;
	}
	//PIE で複数ウィンドウを開くと表示は全ウィンドウ共通なので、どちらのプレイヤーか頭に付ける
	const TCHAR* Prefix = CharacterOwner->HasAuthority() ? TEXT("[Host]") : TEXT("[Client]");
	GEngine->AddOnScreenDebugMessage(-1, 3.0f, Color, FString::Printf(TEXT("%s %s"), Prefix, *Text), true, FVector2D(1.3f, 1.3f));
#endif
}

void USavaCharacterMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

#if !UE_BUILD_SHIPPING
	if (!bShowTechniqueDebug || !GEngine || !CharacterOwner || !CharacterOwner->IsLocallyControlled())
	{
		return;
	}

	//毎フレーム同じ行を書き換える状態表示: 速度・移動の種類・進行中のテクニック
	const FString Mode = MovementMode == MOVE_Custom
		? StaticEnum<ESavaCustomMovementMode>()->GetDisplayNameTextByValue(CustomMovementMode).ToString()
		: GetMovementName();
	FString Line = FString::Printf(TEXT("%s Speed %4.0f  Up %5.0f  |  %s"),
		CharacterOwner->HasAuthority() ? TEXT("[Host]") : TEXT("[Client]"), Velocity.Size2D(), Velocity.Z, *Mode);

	if (IsWallRunning())
	{
		Line += FString::Printf(TEXT("  |  Tap window %.2fs  |  Wall time left %.2fs%s"),
			FMath::Max(WallTapWindow - WallTapElapsed, 0.0f),
			FMath::Max(WallRunMaxDuration - WallRunElapsed, 0.0f),
			bWallRunOnSameWall ? TEXT(" (same wall)") : TEXT(""));
	}
	//壁走り中と、着地前にウォールジャンプした後は、次のウォールジャンプの上向き速度を出す
	if (IsWallRunning() || WallJumpDecay > 0.0f)
	{
		Line += FString::Printf(TEXT("  |  Next WJ up %+.0f"), GetNextWallJumpUpSpeed());
	}
	if (IsFalling() && CharacterOwner->CanJump())
	{
		Line += TEXT("  |  DOUBLE JUMP READY");
	}
	if (bJumpHeld)
	{
		Line += TEXT("  |  JUMP HELD");
	}
	if (WallRunStartDelayRemaining > 0.0f)
	{
		Line += FString::Printf(TEXT("  |  Wall run blocked (ground jump) %.2fs"), WallRunStartDelayRemaining);
	}
	if (WallJumpLateReleaseRemaining > 0.0f)
	{
		Line += FString::Printf(TEXT("  |  Late wall jump %.2fs"), WallJumpLateReleaseRemaining);
	}
	if (JustLandingInputRemaining > 0.0f)
	{
		Line += FString::Printf(TEXT("  |  Just landing window %.2fs"), JustLandingInputRemaining);
	}
	//ブーストの残りクールダウン(0 なら使える)
	Line += FString::Printf(TEXT("  |  Boost CD: slide %.1fs / just landing %.1fs"), SlideBoostCooldownRemaining, JustLandingCooldownRemaining);
	if (IsSliding())
	{
		//足元の坂の角度(上りがプラス)。クレストジャンプは CrestJumpMinSlopeAngle 以上の上りが必要
		Line += FString::Printf(TEXT("  |  Slope %+.0f deg"), FMath::RadiansToDegrees(FMath::Atan(GetSlopeTanAlongVelocity())));
		if (IsCrestJumpAvailable())
		{
			Line += TEXT("  CREST READY");
		}
	}
	if (LurchTimeRemaining > 0.0f)
	{
		Line += FString::Printf(TEXT("  |  Lurch %.2fs  %.0f deg left"), LurchTimeRemaining, LurchAngleRemaining);
	}
	GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 0.5f, FColor::Yellow, Line, true, FVector2D(1.3f, 1.3f));
#endif
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
		//デバッグ表示: サーバーに位置を直された(自分の画面とサーバーで動きが食い違った)
		ShowTechniqueDebug(TEXT("Server correction"), FColor::Red);

		const FSavaCharacterMoveResponseDataContainer& SavaResponse = static_cast<const FSavaCharacterMoveResponseDataContainer&>(MoveResponse);
		SlideBoostCooldownRemaining = SavaResponse.SlideBoostCooldownRemaining;
		WallRunNormal = SavaResponse.WallRunNormal;
		WallRunElapsed = SavaResponse.WallRunElapsed;
		LastWallRunNormal = SavaResponse.LastWallRunNormal;
		WallTapElapsed = SavaResponse.WallTapElapsed;
		bWallRunOnSameWall = SavaResponse.bWallRunOnSameWall;
		WallJumpDecay = SavaResponse.WallJumpDecay;
		WallRunSameWallCooldownRemaining = SavaResponse.WallRunSameWallCooldownRemaining;
		WallRunStartDelayRemaining = SavaResponse.WallRunStartDelayRemaining;
		WallJumpLateReleaseRemaining = SavaResponse.WallJumpLateReleaseRemaining;
		JustLandingInputRemaining = SavaResponse.JustLandingInputRemaining;
		JustLandingCooldownRemaining = SavaResponse.JustLandingCooldownRemaining;
		bPrevWantsToCrouch = SavaResponse.bPrevWantsToCrouch;
		LurchTimeRemaining = SavaResponse.LurchTimeRemaining;
		LurchAngleRemaining = SavaResponse.LurchAngleRemaining;
		LurchInputDir = SavaResponse.LurchInputDir;
		CrestUpSpeed = SavaResponse.CrestUpSpeed;
		CrestTimeRemaining = SavaResponse.CrestTimeRemaining;
		MantleStartLocation = SavaResponse.MantleStartLocation;
		MantleTargetLocation = SavaResponse.MantleTargetLocation;
		MantleExitDirection = SavaResponse.MantleExitDirection;
		MantleElapsed = SavaResponse.MantleElapsed;
		MantleDuration = SavaResponse.MantleDuration;
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
	WallTapElapsed = SavaMovement.WallTapElapsed;
	bWallRunOnSameWall = SavaMovement.bWallRunOnSameWall;
	WallJumpDecay = SavaMovement.WallJumpDecay;
	WallRunSameWallCooldownRemaining = SavaMovement.WallRunSameWallCooldownRemaining;
	WallRunStartDelayRemaining = SavaMovement.WallRunStartDelayRemaining;
	WallJumpLateReleaseRemaining = SavaMovement.WallJumpLateReleaseRemaining;
	JustLandingInputRemaining = SavaMovement.JustLandingInputRemaining;
	JustLandingCooldownRemaining = SavaMovement.JustLandingCooldownRemaining;
	bPrevWantsToCrouch = SavaMovement.bPrevWantsToCrouch;
	LurchTimeRemaining = SavaMovement.LurchTimeRemaining;
	LurchAngleRemaining = SavaMovement.LurchAngleRemaining;
	LurchInputDir = SavaMovement.LurchInputDir;
	CrestUpSpeed = SavaMovement.CrestUpSpeed;
	CrestTimeRemaining = SavaMovement.CrestTimeRemaining;
	MantleStartLocation = SavaMovement.MantleStartLocation;
	MantleTargetLocation = SavaMovement.MantleTargetLocation;
	MantleExitDirection = SavaMovement.MantleExitDirection;
	MantleElapsed = SavaMovement.MantleElapsed;
	MantleDuration = SavaMovement.MantleDuration;
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
		Ar << WallTapElapsed;
		Ar << bWallRunOnSameWall;
		Ar << WallJumpDecay;
		Ar << WallRunSameWallCooldownRemaining;
		Ar << WallRunStartDelayRemaining;
		Ar << WallJumpLateReleaseRemaining;
		Ar << JustLandingInputRemaining;
		Ar << JustLandingCooldownRemaining;
		Ar << bPrevWantsToCrouch;
		Ar << LurchTimeRemaining;
		Ar << LurchAngleRemaining;
		Ar << LurchInputDir;
		Ar << CrestUpSpeed;
		Ar << CrestTimeRemaining;
		Ar << MantleStartLocation;
		Ar << MantleTargetLocation;
		Ar << MantleExitDirection;
		Ar << MantleElapsed;
		Ar << MantleDuration;
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
	bSavedJumpHeld = false;
	StartCustomState = FSavaCustomMoveState();
}

void USavaCharacterMovementComponent::FSavedMove_Sava::SetInitialPosition(ACharacter* Character)
{
	Super::SetInitialPosition(Character);
	if (const USavaCharacterMovementComponent* SavaMovement = Cast<USavaCharacterMovementComponent>(Character->GetCharacterMovement()))
	{
		StartCustomState = SavaMovement->CaptureCustomMoveState();
	}
}

void USavaCharacterMovementComponent::FSavedMove_Sava::CombineWith(const FSavedMove_Character* OldMove, ACharacter* InCharacter, APlayerController* PC, const FVector& OldStartLocation)
{
	Super::CombineWith(OldMove, InCharacter, PC, OldStartLocation);

	//位置・速度と一緒に独自の状態も古い移動の開始時点へ戻す(戻さないと、結合した移動の再実行でタイマーが二重に進み、サーバーとずれて補正される)
	//この後、呼び出し側が SetInitialPosition を呼び直すので、結合後の移動の開始状態もこの値になる
	USavaCharacterMovementComponent* SavaMovement = Cast<USavaCharacterMovementComponent>(InCharacter->GetCharacterMovement());
	if (SavaMovement)
	{
		SavaMovement->RestoreCustomMoveState(static_cast<const FSavedMove_Sava*>(OldMove)->StartCustomState);
	}
}

USavaCharacterMovementComponent::FSavaCustomMoveState USavaCharacterMovementComponent::CaptureCustomMoveState() const
{
	FSavaCustomMoveState State;
	State.SlideBoostCooldownRemaining = SlideBoostCooldownRemaining;
	State.WallRunNormal = WallRunNormal;
	State.WallRunElapsed = WallRunElapsed;
	State.LastWallRunNormal = LastWallRunNormal;
	State.WallTapElapsed = WallTapElapsed;
	State.bWallRunOnSameWall = bWallRunOnSameWall;
	State.WallJumpDecay = WallJumpDecay;
	State.WallRunSameWallCooldownRemaining = WallRunSameWallCooldownRemaining;
	State.WallRunStartDelayRemaining = WallRunStartDelayRemaining;
	State.WallJumpLateReleaseRemaining = WallJumpLateReleaseRemaining;
	State.JustLandingInputRemaining = JustLandingInputRemaining;
	State.JustLandingCooldownRemaining = JustLandingCooldownRemaining;
	State.bPrevWantsToCrouch = bPrevWantsToCrouch;
	State.LurchTimeRemaining = LurchTimeRemaining;
	State.LurchAngleRemaining = LurchAngleRemaining;
	State.LurchInputDir = LurchInputDir;
	State.CrestUpSpeed = CrestUpSpeed;
	State.CrestTimeRemaining = CrestTimeRemaining;
	State.MantleStartLocation = MantleStartLocation;
	State.MantleTargetLocation = MantleTargetLocation;
	State.MantleExitDirection = MantleExitDirection;
	State.MantleElapsed = MantleElapsed;
	State.MantleDuration = MantleDuration;
	return State;
}

void USavaCharacterMovementComponent::RestoreCustomMoveState(const FSavaCustomMoveState& State)
{
	SlideBoostCooldownRemaining = State.SlideBoostCooldownRemaining;
	WallRunNormal = State.WallRunNormal;
	WallRunElapsed = State.WallRunElapsed;
	LastWallRunNormal = State.LastWallRunNormal;
	WallTapElapsed = State.WallTapElapsed;
	bWallRunOnSameWall = State.bWallRunOnSameWall;
	WallJumpDecay = State.WallJumpDecay;
	WallRunSameWallCooldownRemaining = State.WallRunSameWallCooldownRemaining;
	WallRunStartDelayRemaining = State.WallRunStartDelayRemaining;
	WallJumpLateReleaseRemaining = State.WallJumpLateReleaseRemaining;
	JustLandingInputRemaining = State.JustLandingInputRemaining;
	JustLandingCooldownRemaining = State.JustLandingCooldownRemaining;
	bPrevWantsToCrouch = State.bPrevWantsToCrouch;
	LurchTimeRemaining = State.LurchTimeRemaining;
	LurchAngleRemaining = State.LurchAngleRemaining;
	LurchInputDir = State.LurchInputDir;
	CrestUpSpeed = State.CrestUpSpeed;
	CrestTimeRemaining = State.CrestTimeRemaining;
	MantleStartLocation = State.MantleStartLocation;
	MantleTargetLocation = State.MantleTargetLocation;
	MantleExitDirection = State.MantleExitDirection;
	MantleElapsed = State.MantleElapsed;
	MantleDuration = State.MantleDuration;
}

uint8 USavaCharacterMovementComponent::FSavedMove_Sava::GetCompressedFlags() const
{
	uint8 Result = Super::GetCompressedFlags();
	if (bSavedWantsToSprint)
	{
		Result |= FSavedMove_Character::FLAG_Custom_0;
	}
	if (bSavedJumpHeld)
	{
		Result |= FSavedMove_Character::FLAG_Custom_1;
	}
	return Result;
}

bool USavaCharacterMovementComponent::FSavedMove_Sava::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const
{
	const FSavedMove_Sava* NewSavaMove = static_cast<FSavedMove_Sava*>(NewMove.Get());
	if (bSavedWantsToSprint != NewSavaMove->bSavedWantsToSprint || bSavedJumpHeld != NewSavaMove->bSavedJumpHeld) {
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
		bSavedJumpHeld = SavaMovement->bJumpHeld;
	}
}

void USavaCharacterMovementComponent::FSavedMove_Sava::PrepMoveFor(class ACharacter* Character)
{
	Super::PrepMoveFor(Character);
	USavaCharacterMovementComponent* SavaMovement = Cast<USavaCharacterMovementComponent>(Character->GetCharacterMovement());
	if (SavaMovement) {
		SavaMovement->bWantsToSprint = bSavedWantsToSprint;
		SavaMovement->bJumpHeld = bSavedJumpHeld;
	}
}
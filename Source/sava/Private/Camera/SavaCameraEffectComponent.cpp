// Fill out your copyright notice in the Description page of Project Settings.

#include "Camera/SavaCameraEffectComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "SavaCharacterMovementComponent.h"
#include "SavaGameUserSettings.h"
#include "SavaGrappleRootMotionSource.h"
#include "Skill/SavaBlinkAbility.h"
#include "Weapon/SavaEquipmentComponent.h"
#include "Weapon/SavaWeaponTypes.h"
#include "savaCharacter.h"

//--------------------------------FSavaCameraSpring

void FSavaCameraSpring::AddKick(const FVector& Peak, float PeakTime)
{
	if (Peak.IsNearlyZero())
	{
		return;
	}
	//速さを与えると、PeakTime 後に (速さ / (強さ × e)) まで動いてから戻るので、狙った大きさになる速さを足す
	Stiffness = 1.0f / FMath::Max(PeakTime, 0.005f);
	Velocity += Peak * Stiffness * UE_EULERS_NUMBER;
	bActive = true;
}

void FSavaCameraSpring::Step(float DeltaTime)
{
	if (!bActive)
	{
		return;
	}

	const float W = Stiffness;
	const float Decay = FMath::Exp(-W * DeltaTime);
	const FVector Temp = Velocity + Value * W;
	Value = (Value + Temp * DeltaTime) * Decay;
	Velocity = (Velocity - Temp * (W * DeltaTime)) * Decay;

	//十分小さくなったら止める
	if (Value.IsNearlyZero(0.001f) && Velocity.IsNearlyZero(0.01f))
	{
		Reset();
	}
}

void FSavaCameraSpring::Reset()
{
	Value = FVector::ZeroVector;
	Velocity = FVector::ZeroVector;
	bActive = false;
}

//--------------------------------USavaCameraEffectComponent

USavaCameraEffectComponent::USavaCameraEffectComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void USavaCameraEffectComponent::BeginPlay()
{
	Super::BeginPlay();

	CaptureCameraBaseLocation();
	if (const UCameraComponent* Camera = GetCamera())
	{
		BaseFieldOfView = Camera->FieldOfView;
	}

	//ADS の進み具合を、そのフレームで進めた後の値で使う
	if (USavaEquipmentComponent* Equipment = GetEquipment())
	{
		AddTickPrerequisiteComponent(Equipment);
	}

	if (USavaCharacterMovementComponent* Movement = GetMovement())
	{
		Movement->OnDoubleJumped.AddDynamic(this, &USavaCameraEffectComponent::HandleDoubleJumped);
		Movement->OnWallJumped.AddDynamic(this, &USavaCameraEffectComponent::HandleWallJumped);
	}
}

void USavaCameraEffectComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	//しゃがみの高さは全員の画面で動かす
	UpdateCrouch(DeltaTime);

	//ここから下は、操作している本人の画面だけ
	APlayerController* PlayerController = GetLocalPlayerController();
	if (!PlayerController)
	{
		return;
	}

	//視点の向きを直接動かす演出
	UpdateRecoil(PlayerController, DeltaTime);
	UpdateWallRunTilt(PlayerController, DeltaTime);

	//見た目のずれの演出
	RecoilKickSpring.Step(DeltaTime);
	ImpulseRotationSpring.Step(DeltaTime);
	ImpulseLocationSpring.Step(DeltaTime);
	UpdateHeadBob(DeltaTime);

	if (const AsavaCharacter* Character = GetCharacter())
	{
		const float WallRunSide = Character->GetWallRunSide();
		if (WallRunSide != 0.0f)
		{
			LastWallRunSide = WallRunSide;
		}
	}

	//合成してカメラと腕に反映する
	ApplyFieldOfView(DeltaTime);
	ApplyViewOffsetAndArms();
}

float USavaCameraEffectComponent::GetShakeScale()
{
	const USavaGameUserSettings* Settings = USavaGameUserSettings::GetSavaGameUserSettings();
	return Settings ? Settings->GetCameraShakeScale() : 1.0f;
}

//--------------------------------しゃがみ

void USavaCameraEffectComponent::NotifyStartCrouch(float ScaledHalfHeightAdjust)
{
	//カプセルの上端は中心より ScaledHalfHeightAdjust だけ下がるので、カメラも同じだけ下げる
	//(下げないと、しゃがんだときに目がカプセルの上に出て、自分は撃てるのに撃たれない高さができる)
	CrouchCameraDrop = ScaledHalfHeightAdjust;
	float CameraJump = ScaledHalfHeightAdjust;

	//地上ではカプセルが足元基準で縮み、中心も一瞬で下がる
	const AsavaCharacter* Character = GetCharacter();
	if (Character && Character->GetCharacterMovement()->bCrouchMaintainsBaseLocation)
	{
		CameraJump += ScaledHalfHeightAdjust;
	}

	//一瞬で下がった分を打ち消して、元の高さから補間させる
	CrouchCameraOffset.Z += CameraJump;
	ApplyCameraLocation();
}

void USavaCameraEffectComponent::NotifyEndCrouch(float ScaledHalfHeightAdjust)
{
	CrouchCameraDrop = 0.0f;
	float CameraJump = ScaledHalfHeightAdjust;

	const AsavaCharacter* Character = GetCharacter();
	if (Character && Character->GetCharacterMovement()->bCrouchMaintainsBaseLocation)
	{
		CameraJump += ScaledHalfHeightAdjust;
	}

	CrouchCameraOffset.Z -= CameraJump;
	ApplyCameraLocation();
}

void USavaCameraEffectComponent::SetEyeHeight(float Height)
{
	//BeginPlay 前(複製が先に届いた)ならまだ控えがないので、今の位置を元にする
	CaptureCameraBaseLocation();
	CameraBaseLocation.Z = Height;
	CrouchCameraOffset = FVector::ZeroVector;
	ApplyCameraLocation();
}

void USavaCameraEffectComponent::UpdateCrouch(float DeltaTime)
{
	if (CrouchCameraOffset.IsZero())
	{
		return;
	}
	CrouchCameraOffset = FMath::VInterpTo(CrouchCameraOffset, FVector::ZeroVector, DeltaTime, CrouchCameraInterpSpeed);
	ApplyCameraLocation();
}

//--------------------------------反動

void USavaCameraEffectComponent::AddRecoil(const FVector2D& Kick, const FSavaWeaponStats& Stats)
{
	APlayerController* PlayerController = GetLocalPlayerController();
	if (!PlayerController)
	{
		return;
	}

	//視点へは Recoil Kick Time かけて Tick で足す
	PendingRecoil += Kick;

	//見た目だけの揺れ: Visual Kick Time 後に一番大きくなり、バネのように戻る
	RecoilKickSpring.AddKick(FVector(Kick.Y * Stats.VisualKickScale, Kick.X * Stats.VisualKickScale,
		FMath::FRandRange(-Stats.VisualKickRoll, Stats.VisualKickRoll)), Stats.VisualKickTime);

	RecoilKickTimeRemaining = Stats.RecoilKickTime;
	if (RecoilKickTimeRemaining <= 0.0f)
	{
		AddRecoilToView(PlayerController, PendingRecoil);
		PendingRecoil = FVector2D::ZeroVector;
	}

	LastRecoilTime = GetWorld()->GetTimeSeconds();
	RecoilRecoveryDelay = Stats.GetFireInterval();
	RecoilRecoverySpeed = Stats.RecoilRecovery;
}

void USavaCameraEffectComponent::UpdateRecoil(APlayerController* PlayerController, float DeltaTime)
{
	//溜まった反動を足す。残りの割合を (残り時間 / Kick Time)^2 で減らすので、最初は速く、だんだんゆっくりになり、ちょうど Kick Time で足し終わる
	//(撃つたびに残り時間は Kick Time に戻り、前の 1 発の残りもまとめて足す)
	if (RecoilKickTimeRemaining > 0.0f)
	{
		const float NewRemaining = FMath::Max(RecoilKickTimeRemaining - DeltaTime, 0.0f);
		const float RemainingRatio = FMath::Square(NewRemaining / RecoilKickTimeRemaining);
		const FVector2D Step = PendingRecoil * (1.0f - RemainingRatio);
		AddRecoilToView(PlayerController, Step);
		PendingRecoil -= Step;
		RecoilKickTimeRemaining = NewRemaining;
		if (RecoilKickTimeRemaining <= 0.0f)
		{
			PendingRecoil = FVector2D::ZeroVector;
		}
		return;
	}

	if (RecoilPitchToRecover <= 0.0f)
	{
		return;
	}

	//撃ち続けている間は戻さない(反動を足し終え、次の 1 発までの間隔が過ぎたら戻し始める)
	if (GetWorld()->GetTimeSeconds() - LastRecoilTime < RecoilRecoveryDelay)
	{
		return;
	}

	FRotator ControlRotation = PlayerController->GetControlRotation();
	const float CurrentPitch = FRotator::NormalizeAxis(ControlRotation.Pitch);
	AbsorbPlayerPitchInput(CurrentPitch);

	const float Step = FMath::Min(RecoilPitchToRecover, RecoilRecoverySpeed * DeltaTime);
	if (Step <= 0.0f)
	{
		return;
	}
	ControlRotation.Pitch = CurrentPitch - Step;
	PlayerController->SetControlRotation(ControlRotation);

	RecoilPitchToRecover -= Step;
	LastRecoilPitch = ControlRotation.Pitch;
}

void USavaCameraEffectComponent::AddRecoilToView(APlayerController* PlayerController, const FVector2D& Kick)
{
	//角度を上書きせず差分を足すので、反動を足している間もプレイヤーの視点操作はそのまま効く
	FRotator ControlRotation = PlayerController->GetControlRotation();
	const float CurrentPitch = FRotator::NormalizeAxis(ControlRotation.Pitch);
	AbsorbPlayerPitchInput(CurrentPitch);

	ControlRotation.Pitch = CurrentPitch + Kick.Y;
	ControlRotation.Yaw += Kick.X;
	PlayerController->SetControlRotation(ControlRotation);

	//下向きの反動の分は、戻す量から引く
	RecoilPitchToRecover = FMath::Max(0.0f, RecoilPitchToRecover + Kick.Y);
	LastRecoilPitch = ControlRotation.Pitch;
}

void USavaCameraEffectComponent::AbsorbPlayerPitchInput(float CurrentPitch)
{
	if (RecoilPitchToRecover <= 0.0f)
	{
		return;
	}
	const float PlayerDelta = FRotator::NormalizeAxis(CurrentPitch - LastRecoilPitch);
	if (PlayerDelta < 0.0f)
	{
		RecoilPitchToRecover = FMath::Max(0.0f, RecoilPitchToRecover + PlayerDelta);
	}
}

//--------------------------------移動

void USavaCameraEffectComponent::NotifyLanded(float FallSpeed)
{
	if (!GetLocalPlayerController())
	{
		return;
	}

	const float Alpha = CalculateLandingAlpha(FallSpeed, LandingMinFallSpeed, LandingMaxFallSpeed);
	if (Alpha <= 0.0f)
	{
		return;
	}
	ImpulseLocationSpring.AddKick(FVector(0.0f, 0.0f, -LandingMaxDrop * Alpha), LandingKickTime);
	ImpulseRotationSpring.AddKick(FVector(-LandingMaxPitch * Alpha, 0.0f, 0.0f), LandingKickTime);
}

float USavaCameraEffectComponent::CalculateLandingAlpha(float FallSpeed, float MinSpeed, float MaxSpeed)
{
	if (FallSpeed < MinSpeed)
	{
		return 0.0f;
	}
	if (MaxSpeed <= MinSpeed)
	{
		return 1.0f;
	}
	return FMath::Clamp((FallSpeed - MinSpeed) / (MaxSpeed - MinSpeed), 0.0f, 1.0f);
}

void USavaCameraEffectComponent::HandleDoubleJumped()
{
	if (!GetLocalPlayerController())
	{
		return;
	}
	ImpulseRotationSpring.AddKick(FVector(-DoubleJumpKickPitch, 0.0f, 0.0f), JumpKickTime);
	ImpulseLocationSpring.AddKick(FVector(0.0f, 0.0f, -DoubleJumpKickDrop), JumpKickTime);
}

void USavaCameraEffectComponent::HandleWallJumped()
{
	if (!GetLocalPlayerController())
	{
		return;
	}
	//壁走りの傾きと同じ向き(壁と反対側)へ、さらに少し傾ける
	ImpulseRotationSpring.AddKick(FVector(0.0f, 0.0f, -LastWallRunSide * WallJumpKickRoll), JumpKickTime);
}

void USavaCameraEffectComponent::UpdateHeadBob(float DeltaTime)
{
	//地上を歩いている間だけ(スライディング・壁走り・空中では揺らさない)
	float TargetAlpha = 0.0f;
	const USavaCharacterMovementComponent* Movement = GetMovement();
	if (Movement && (Movement->MovementMode == MOVE_Walking || Movement->MovementMode == MOVE_NavWalking))
	{
		const float Speed = Movement->Velocity.Size2D();
		TargetAlpha = FMath::Min(Speed / HeadBobReferenceSpeed, HeadBobMaxScale);
		//2 歩で 1 周する
		HeadBobPhase = FMath::Fmod(HeadBobPhase + Speed * DeltaTime / (2.0f * HeadBobStepLength) * UE_TWO_PI, UE_TWO_PI);
	}

	//ADS 中は弱める
	const USavaEquipmentComponent* Equipment = GetEquipment();
	const float ADSViewAlpha = Equipment ? Equipment->GetADSViewAlpha() : 0.0f;
	TargetAlpha *= FMath::Lerp(1.0f, HeadBobADSScale, ADSViewAlpha);

	HeadBobAlpha = FMath::FInterpTo(HeadBobAlpha, TargetAlpha, DeltaTime, HeadBobBlendSpeed);
}

//--------------------------------壁走り

void USavaCameraEffectComponent::UpdateWallRunTilt(APlayerController* PlayerController, float DeltaTime)
{
	//視点の回転に傾き(ロール)を足す。カメラに付いた腕も一緒に傾き、狙う方向には影響しない
	const AsavaCharacter* Character = GetCharacter();
	if (!Character)
	{
		return;
	}

	//右の壁なら左へ、左の壁なら右へ傾ける
	const float TargetRoll = -Character->GetWallRunSide() * WallRunCameraTiltAngle;
	FRotator ControlRotation = PlayerController->GetControlRotation();
	const float CurrentRoll = FRotator::NormalizeAxis(ControlRotation.Roll);
	if (CurrentRoll == TargetRoll)
	{
		return;
	}

	ControlRotation.Roll = FMath::FInterpTo(CurrentRoll, TargetRoll, DeltaTime, WallRunCameraTiltSpeed);
	PlayerController->SetControlRotation(ControlRotation);
}

//--------------------------------視野角

float USavaCameraEffectComponent::CalculateTargetMoveFOVScale() const
{
	float Scale = 1.0f;
	if (const USavaCharacterMovementComponent* Movement = GetMovement())
	{
		if (HasRootMotionSource(USavaBlinkAbility::GetBlinkInstanceName()))
		{
			Scale *= BlinkFOVScale;
		}
		else if (HasRootMotionSource(FSavaGrappleRootMotionSource::GetPullInstanceName()))
		{
			Scale *= GrappleFOVScale;
		}
		else if (Movement->IsSliding())
		{
			Scale *= SlideFOVScale;
		}
		else if (Movement->IsSprinting())
		{
			Scale *= SprintFOVScale;
		}
	}
	return Scale;
}

bool USavaCameraEffectComponent::HasRootMotionSource(FName InstanceName) const
{
	const USavaCharacterMovementComponent* Movement = GetMovement();
	return Movement && Movement->CurrentRootMotion.RootMotionSources.ContainsByPredicate([InstanceName](const TSharedPtr<FRootMotionSource>& Source)
	{
		return Source.IsValid() && Source->InstanceName == InstanceName;
	});
}

//--------------------------------合成

void USavaCameraEffectComponent::ApplyCameraLocation()
{
	if (UCameraComponent* Camera = GetCamera())
	{
		CaptureCameraBaseLocation();
		Camera->SetRelativeLocation(CameraBaseLocation - FVector(0.0f, 0.0f, CrouchCameraDrop) + CrouchCameraOffset);
	}
}

void USavaCameraEffectComponent::ApplyFieldOfView(float DeltaTime)
{
	UCameraComponent* Camera = GetCamera();
	if (!Camera || BaseFieldOfView <= 0.0f)
	{
		return;
	}

	//ブリンクは一瞬なので速く広げる(戻るのはいつもの速さ)
	const float InterpSpeed = HasRootMotionSource(USavaBlinkAbility::GetBlinkInstanceName()) ? BlinkFOVInterpSpeed : MoveFOVInterpSpeed;
	MoveFOVScale = FMath::FInterpTo(MoveFOVScale, CalculateTargetMoveFOVScale(), DeltaTime, InterpSpeed);

	//ADS 中は、動きによる倍率を ADS の進み具合の分だけ弱める(ズームを邪魔しない)
	const USavaEquipmentComponent* Equipment = GetEquipment();
	const float ADSScale = Equipment ? Equipment->GetADSFieldOfViewScale() : 1.0f;
	const float ADSViewAlpha = Equipment ? Equipment->GetADSViewAlpha() : 0.0f;
	Camera->SetFieldOfView(BaseFieldOfView * ADSScale * FMath::Lerp(MoveFOVScale, 1.0f, ADSViewAlpha));
}

FTransform USavaCameraEffectComponent::CalculateViewOffset() const
{
	//X = 縦、Y = 横、Z = 傾き(度)
	FVector Rotation = FVector::ZeroVector;
	//X = 前、Y = 右、Z = 上(cm。カメラから見た向き)
	FVector Location = FVector::ZeroVector;

	//衝撃
	Rotation += ImpulseRotationSpring.Value;
	Location += ImpulseLocationSpring.Value;

	//歩行の揺れ(1 歩ごとに上下、2 歩で左右に 1 往復)
	if (HeadBobAlpha > 0.0f)
	{
		const float Side = FMath::Sin(HeadBobPhase);
		Location += FVector(0.0f, Side * HeadBobHorizontalAmplitude, FMath::Sin(2.0f * HeadBobPhase) * HeadBobVerticalAmplitude) * HeadBobAlpha;
		Rotation.Z += Side * HeadBobRollAmplitude * HeadBobAlpha;
	}

	//ここまでが設定の「画面の揺れの強さ」を掛けるもの
	const float ShakeScale = GetShakeScale();
	Rotation *= ShakeScale;
	Location *= ShakeScale;

	//反動の見た目の揺れ(射撃の手応えなので、揺れの強さは掛けない)
	Rotation += RecoilKickSpring.Value;

	return FTransform(FRotator(Rotation.X, Rotation.Y, Rotation.Z), Location);
}

void USavaCameraEffectComponent::ApplyViewOffsetAndArms()
{
	UCameraComponent* Camera = GetCamera();
	const AsavaCharacter* Character = GetCharacter();
	USkeletalMeshComponent* Arms = Character ? Character->GetMesh1P() : nullptr;
	if (!Camera || !Arms)
	{
		return;
	}

	//画面の見た目だけのずれ。視点の向き(Control Rotation)は変えないので、弾の向きは変わらない
	const FTransform ViewOffset = CalculateViewOffset();
	const bool bViewOffsetActive = !ViewOffset.GetRotation().IsIdentity(1.0e-6f) || !ViewOffset.GetTranslation().IsNearlyZero(0.001f);
	if (bViewOffsetApplied)
	{
		Camera->ClearAdditiveOffset();
	}
	if (bViewOffsetActive)
	{
		Camera->AddAdditiveOffset(ViewOffset, 0.0f);
	}
	bViewOffsetApplied = bViewOffsetActive;

	//ずれも ADS もない間は、腕に触らない(元の位置を覚え直すだけ)
	if (!bArmsModified)
	{
		ArmsBaseTransform = Arms->GetRelativeTransform();
	}
	const USavaEquipmentComponent* Equipment = GetEquipment();
	const float ADSViewAlpha = Equipment ? Equipment->GetADSViewAlpha() : 0.0f;
	const FVector ADSOffset = ADSViewAlpha > 0.0f ? Equipment->CalculateADSArmsOffset(Arms, ArmsBaseTransform) * ADSViewAlpha : FVector::ZeroVector;
	const bool bADSOffsetActive = !ADSOffset.IsNearlyZero(0.01f);
	if (!bViewOffsetActive && !bADSOffsetActive && !bArmsModified)
	{
		return;
	}

	//腕は画面と同じだけ動かす。ADS の分は、カメラから見た向きに腕をずらす(腕はカメラに付いているので、相対位置に足す)
	FTransform ArmsTransform = bViewOffsetActive ? ArmsBaseTransform * ViewOffset : ArmsBaseTransform;
	ArmsTransform.AddToTranslation(ADSOffset);
	Arms->SetRelativeTransform(ArmsTransform);

	//元の位置に戻しきったら、次のフレームからは触らない
	bArmsModified = bViewOffsetActive || bADSOffsetActive;
}

//--------------------------------Helper

AsavaCharacter* USavaCameraEffectComponent::GetCharacter() const
{
	return Cast<AsavaCharacter>(GetOwner());
}

UCameraComponent* USavaCameraEffectComponent::GetCamera() const
{
	const AsavaCharacter* Character = GetCharacter();
	return Character ? Character->GetFirstPersonCameraComponent() : nullptr;
}

USavaEquipmentComponent* USavaCameraEffectComponent::GetEquipment() const
{
	const AsavaCharacter* Character = GetCharacter();
	return Character ? Character->GetEquipmentComponent() : nullptr;
}

USavaCharacterMovementComponent* USavaCameraEffectComponent::GetMovement() const
{
	const AsavaCharacter* Character = GetCharacter();
	return Character ? Character->GetSavaCharacterMovementComponent() : nullptr;
}

APlayerController* USavaCameraEffectComponent::GetLocalPlayerController() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Pawn && Pawn->IsLocallyControlled() ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
}

void USavaCameraEffectComponent::CaptureCameraBaseLocation()
{
	if (bHasCameraBaseLocation)
	{
		return;
	}
	if (const UCameraComponent* Camera = GetCamera())
	{
		CameraBaseLocation = Camera->GetRelativeLocation();
		bHasCameraBaseLocation = true;
	}
}

// Fill out your copyright notice in the Description page of Project Settings.

#include "Camera/SavaCameraEffectComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Weapon/SavaEquipmentComponent.h"
#include "Weapon/SavaWeaponTypes.h"
#include "savaCharacter.h"

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

	//演出ごとに状態を進める(視点の向きを直接動かすものは、ここで反映される)
	UpdateRecoil(PlayerController, DeltaTime);
	UpdateWallRunTilt(PlayerController, DeltaTime);
	UpdateVisualKick(DeltaTime);

	//合成してカメラと腕に反映する
	ApplyFieldOfView();
	ApplyViewKickAndArms();
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

	//見た目だけの揺れ: 速さを与えると、Visual Kick Time 後に (速さ / (強さ × e)) まで跳ねてから戻るので、狙った大きさになる速さを足す
	const FVector VisualKick(Kick.Y * Stats.VisualKickScale, Kick.X * Stats.VisualKickScale,
		FMath::FRandRange(-Stats.VisualKickRoll, Stats.VisualKickRoll));
	if (!VisualKick.IsNearlyZero())
	{
		VisualKickStiffness = 1.0f / FMath::Max(Stats.VisualKickTime, 0.005f);
		VisualKickVelocity += VisualKick * VisualKickStiffness * UE_EULERS_NUMBER;
		bVisualKickActive = true;
	}

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

//--------------------------------見た目だけの揺れ

void USavaCameraEffectComponent::UpdateVisualKick(float DeltaTime)
{
	if (!bVisualKickActive)
	{
		return;
	}

	//行きすぎない一番速いバネ(臨界減衰)の式で進める(フレームの長さによらず同じ動きになる)
	const float W = VisualKickStiffness;
	const float Decay = FMath::Exp(-W * DeltaTime);
	const FVector Temp = VisualKickVelocity + VisualKickOffset * W;
	VisualKickOffset = (VisualKickOffset + Temp * DeltaTime) * Decay;
	VisualKickVelocity = (VisualKickVelocity - Temp * (W * DeltaTime)) * Decay;

	//十分小さくなったら止める
	if (VisualKickOffset.IsNearlyZero(0.001f) && VisualKickVelocity.IsNearlyZero(0.01f))
	{
		VisualKickOffset = FVector::ZeroVector;
		VisualKickVelocity = FVector::ZeroVector;
		bVisualKickActive = false;
	}
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

//--------------------------------合成

void USavaCameraEffectComponent::ApplyCameraLocation()
{
	if (UCameraComponent* Camera = GetCamera())
	{
		CaptureCameraBaseLocation();
		Camera->SetRelativeLocation(CameraBaseLocation - FVector(0.0f, 0.0f, CrouchCameraDrop) + CrouchCameraOffset);
	}
}

void USavaCameraEffectComponent::ApplyFieldOfView()
{
	UCameraComponent* Camera = GetCamera();
	if (!Camera || BaseFieldOfView <= 0.0f)
	{
		return;
	}

	const USavaEquipmentComponent* Equipment = GetEquipment();
	const float ADSScale = Equipment ? Equipment->GetADSFieldOfViewScale() : 1.0f;
	Camera->SetFieldOfView(BaseFieldOfView * ADSScale);
}

void USavaCameraEffectComponent::ApplyViewKickAndArms()
{
	UCameraComponent* Camera = GetCamera();
	const AsavaCharacter* Character = GetCharacter();
	USkeletalMeshComponent* Arms = Character ? Character->GetMesh1P() : nullptr;
	if (!Camera || !Arms)
	{
		return;
	}

	//画面の見た目だけの回転。視点の向き(Control Rotation)は変えないので、弾の向きは変わらない
	const FTransform Kick(FRotator(VisualKickOffset.X, VisualKickOffset.Y, VisualKickOffset.Z));
	if (bViewKickApplied)
	{
		Camera->ClearAdditiveOffset();
	}
	if (bVisualKickActive)
	{
		Camera->AddAdditiveOffset(Kick, 0.0f);
	}
	bViewKickApplied = bVisualKickActive;

	//揺れても ADS でもない間は、腕に触らない(元の位置を覚え直すだけ)
	if (!bArmsModified)
	{
		ArmsBaseTransform = Arms->GetRelativeTransform();
	}
	const USavaEquipmentComponent* Equipment = GetEquipment();
	const float ADSViewAlpha = Equipment ? Equipment->GetADSViewAlpha() : 0.0f;
	const FVector ADSOffset = ADSViewAlpha > 0.0f ? Equipment->CalculateADSArmsOffset(Arms, ArmsBaseTransform) * ADSViewAlpha : FVector::ZeroVector;
	const bool bADSOffsetActive = !ADSOffset.IsNearlyZero(0.01f);
	if (!bVisualKickActive && !bADSOffsetActive && !bArmsModified)
	{
		return;
	}

	//腕は画面と同じだけ回す。ADS の分は、カメラから見た向きに腕をずらす(腕はカメラに付いているので、相対位置に足す)
	FTransform ArmsTransform = bVisualKickActive ? ArmsBaseTransform * Kick : ArmsBaseTransform;
	ArmsTransform.AddToTranslation(ADSOffset);
	Arms->SetRelativeTransform(ArmsTransform);

	//元の位置に戻しきったら、次のフレームからは触らない
	bArmsModified = bVisualKickActive || bADSOffsetActive;
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

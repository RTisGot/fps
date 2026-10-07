#include "Skill/SavaBlinkAbility.h"

#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/RootMotionSource.h"
#include "NativeGameplayTags.h"
#include "SavaCharacterMovementComponent.h"
#include "SavaGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Cooldown_Blink, "Cooldown.Skill.Blink");

namespace SavaBlink
{
	// 自分の画面とサーバーで同じ移動先にするため、1 cm 単位に丸める(通信で値が少し変わっても一致する)
	FVector RoundLocation(const FVector& Location)
	{
		return FVector(FMath::RoundToDouble(Location.X), FMath::RoundToDouble(Location.Y), FMath::RoundToDouble(Location.Z));
	}

	// カプセルの当たり判定(キャラクターの移動と同じ設定)で調べるための準備
	void InitCapsuleQuery(const ACharacter* Character, FCollisionQueryParams& OutParams, FCollisionResponseParams& OutResponseParams)
	{
		OutParams = FCollisionQueryParams(SCENE_QUERY_STAT(SavaBlink), false, Character);
		Character->GetCapsuleComponent()->InitSweepCollisionParams(OutParams, OutResponseParams);
	}
}

USavaBlinkAbility::USavaBlinkAbility()
{
	AimMode = ESavaAimMode::PointOnSurface;
	MaxRange = 1500.0f;
	CooldownDuration = FScalableFloat(8.0f);
	CooldownTags.AddTag(TAG_Cooldown_Blink);

	// 狙っている間に右クリックでキャンセル
	CancelInputTag = SavaGameplayTags::InputTag_Weapon_Aim;

	// 能力の種類
	FGameplayTagContainer Tags = GetAssetTags();
	Tags.AddTag(SavaGameplayTags::Ability_Type_Skill);
	SetAssetTags(Tags);
}

//--------------------------------移動先を探す

bool USavaBlinkAbility::FindBlinkTarget(const ACharacter* Character, const FVector& ViewLocation, const FVector& ViewDirection,
	float Range, float SurfaceClearance, float MinDistance, FVector& OutTarget)
{
	const UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	const FVector Direction = ViewDirection.GetSafeNormal();
	if (!Capsule || !World || Direction.IsNearlyZero())
	{
		return false;
	}

	const FVector Start = Character->GetActorLocation();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();

	// 1. 視線の先。何もなければまっすぐ Range 先、壁・床に当たったらカプセルが収まる位置
	FVector Desired = Start + Direction * Range;
	FHitResult AimHit;
	const FCollisionQueryParams AimParams(SCENE_QUERY_STAT(SavaBlinkAim), false, Character);
	if (World->LineTraceSingleByChannel(AimHit, ViewLocation, ViewLocation + Direction * Range, ECC_Visibility, AimParams))
	{
		// 面の向きに合わせて離す距離を変える(床なら半分の高さ、壁なら半径)
		const FVector Normal = AimHit.ImpactNormal;
		const float Support = Radius + (HalfHeight - Radius) * FMath::Abs(Normal.Z);
		Desired = AimHit.ImpactPoint + Normal * (Support + SurfaceClearance);

		// 壁のときは、目の高さが狙った点に来るようにする(床のときは床に立つ高さのまま)
		const float EyeHeight = ViewLocation.Z - Start.Z;
		Desired.Z -= EyeHeight * (1.0f - FMath::Abs(Normal.Z));
	}
	// 移動距離は最大 Range
	Desired = Start + (Desired - Start).GetClampedToMaxSize(Range);

	// 2. 今の位置からカプセルを滑らせて、途中の壁で止める(壁を通り抜けない)
	//    少し細いカプセルで調べる(立っている床・寄りかかっている壁に最初から当たらないように)
	constexpr float SweepShrink = 2.0f;
	FCollisionQueryParams Params;
	FCollisionResponseParams ResponseParams;
	SavaBlink::InitCapsuleQuery(Character, Params, ResponseParams);

	FVector Target = Desired;
	FHitResult SweepHit;
	if (World->SweepSingleByChannel(SweepHit, Start, Desired, Capsule->GetComponentQuat(), Capsule->GetCollisionObjectType(),
		Capsule->GetCollisionShape(-SweepShrink), Params, ResponseParams))
	{
		Target = SweepHit.Location + SweepHit.Normal * (SweepShrink + 1.0f);
	}
	OutTarget = SavaBlink::RoundLocation(Target);

	return !SweepHit.bStartPenetrating && FVector::Dist(Start, OutTarget) >= MinDistance;
}

bool USavaBlinkAbility::ComputeAim(FTransform& OutTransform, TArray<FVector>& OutPathPoints) const
{
	OutPathPoints.Reset();

	const ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const USavaCharacterMovementComponent* Movement = GetSavaMovementFromActorInfo();
	const AController* Controller = Character ? Character->GetController() : nullptr;
	if (!Movement || !Controller)
	{
		return false;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);

	FVector Target;
	const bool bFound = FindBlinkTarget(Character, ViewLocation, ViewRotation.Vector(), MaxRange, SurfaceClearance, MinBlinkDistance, Target);
	OutTransform = FTransform(FRotator(0.0f, ViewRotation.Yaw, 0.0f), Target);

	// よじ登り中・動けない状態では使えない
	return bFound && !Movement->IsMantling() && Movement->MovementMode != MOVE_None;
}

bool USavaBlinkAbility::IsTargetAllowed_Implementation(const FTransform& TargetTransform) const
{
	// 自分の画面とサーバーの両方で呼ばれる(サーバーでは、クライアントから届いた移動先の確認になる)
	const ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!Capsule || !World)
	{
		return false;
	}

	FCollisionQueryParams Params;
	FCollisionResponseParams ResponseParams;
	SavaBlink::InitCapsuleQuery(Character, Params, ResponseParams);
	const ECollisionChannel Channel = Capsule->GetCollisionObjectType();
	const FVector Target = TargetTransform.GetLocation();

	// 移動先にカプセルが収まるか
	if (World->OverlapBlockingTestByChannel(Target, Capsule->GetComponentQuat(), Channel, Capsule->GetCollisionShape(-1.0f), Params, ResponseParams))
	{
		return false;
	}

	// 今の位置から移動先までの間に壁が無いか(壁抜けの防止)
	return !World->LineTraceTestByChannel(Character->GetActorLocation(), Target, Channel, Params, ResponseParams);
}

//--------------------------------プレビュー

void USavaBlinkAbility::OnAimUpdated_Implementation(const FTransform& AimTransform, bool bIsValid, const TArray<FVector>& PathPoints)
{
	// 自分の画面だけで、狙っている間ずっと(毎フレーム)呼ばれる
	if (!bDrawDebugPreview)
	{
		return;
	}

	const ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const FColor Color = bIsValid ? FColor::Cyan : FColor::Red;
	const FVector Target = AimTransform.GetLocation();

	// 移動後の自分の大きさのカプセルと、足元の印(寿命 -1 = 1 フレームだけ表示。毎フレーム描き直す)
	DrawDebugCapsule(World, Target, Capsule->GetScaledCapsuleHalfHeight(), Capsule->GetScaledCapsuleRadius(),
		FQuat::Identity, Color, false, -1.0f, 0, 1.5f);
	const FVector Feet = Target - FVector(0.0f, 0.0f, Capsule->GetScaledCapsuleHalfHeight());
	DrawDebugCircle(World, Feet, Capsule->GetScaledCapsuleRadius(), 24, Color, false, -1.0f, 0, 2.0f,
		FVector::XAxisVector, FVector::YAxisVector, false);
}

//--------------------------------移動

void USavaBlinkAbility::OnConfirmed_Implementation(const FTransform& TargetTransform)
{
	// サーバー(とリッスンサーバーのホスト・一人プレイ)
	StartBlink(TargetTransform);
}

void USavaBlinkAbility::OnConfirmedPredicted_Implementation(const FTransform& TargetTransform)
{
	// 操作しているクライアント。サーバーの確認を待たずに先に動く
	StartBlink(TargetTransform);
}

void USavaBlinkAbility::StartBlink(const FTransform& TargetTransform)
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	USavaCharacterMovementComponent* Movement = GetSavaMovementFromActorInfo();
	if (!Character || !Movement)
	{
		return;
	}

	const FVector From = Character->GetActorLocation();
	const FVector To = SavaBlink::RoundLocation(TargetTransform.GetLocation());

	// スライディング・壁走りなどの独自移動中は、普通の空中の状態にしてから動かす
	if (Movement->MovementMode == MOVE_Custom)
	{
		Movement->SetMovementMode(MOVE_Falling);
	}

	// Root Motion Source: 「この時間でここへ動かす」という移動の指示。CharacterMovement が通信の予測・補正もしてくれる
	// (自分の画面とサーバーで同じ指示を出すと、同じものとして扱われる。名前・時間・移動先を一致させること)
	const TSharedPtr<FRootMotionSource_MoveToForce> Blink = MakeShared<FRootMotionSource_MoveToForce>();
	Blink->InstanceName = TEXT("SavaBlink");
	Blink->AccumulateMode = ERootMotionAccumulateMode::Override;
	// グラップルの引き寄せ(1000)より優先する
	Blink->Priority = 2000;
	Blink->StartLocation = From;
	Blink->TargetLocation = To;
	Blink->Duration = BlinkDuration;
	Blink->bRestrictSpeedToExpected = false;

	// 移動が終わったら、移動前の速度に戻す(慣性を残す)
	Blink->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::SetVelocity;
	Blink->FinishVelocityParams.SetVelocity = Movement->Velocity;

	Movement->ApplyRootMotionSource(Blink);

	OnBlinkStarted(From, To);
}

#include "Gadget/GadgetPlaceAbility.h"

#include "Gadget/SlowField.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "SavaGameplayTags.h"

UGadgetPlaceAbility::UGadgetPlaceAbility()
{
	// 狙う位置は ComputeAim で自前で計算する。
	// PointOnSurface にしておくと、サーバーの確認で「Max Range 以内か」を見てくれる
	AimMode = ESavaAimMode::PointOnSurface;

	// 置く距離(リーチ)
	MaxRange = 600.0f;

	// 所持数。Commit Ability(= 置いた瞬間)に 1 減る
	MaxCharges = 3;

	// 狙っている間に右クリックでキャンセル
	CancelInputTag = SavaGameplayTags::InputTag_Weapon_Aim;

	PlaceClass = ASlowField::StaticClass();

	// 能力の種類
	FGameplayTagContainer Tags = GetAssetTags();
	Tags.AddTag(SavaGameplayTags::Ability_Type_Gadget);
	SetAssetTags(Tags);
}

FVector UGadgetPlaceAbility::CalculatePlaceLocation(
	const FVector& ViewLocation,
	const FVector& Direction,
	float Reach,
	float HitDistance,
	float WallOffset)
{
	float Distance = Reach;
	if (HitDistance >= 0.0f)
	{
		Distance = FMath::Clamp(HitDistance - WallOffset, 0.0f, Reach);
	}
	return ViewLocation + Direction.GetSafeNormal() * Distance;
}

bool UGadgetPlaceAbility::ComputeAim(FTransform& OutTransform, TArray<FVector>& OutPathPoints) const
{
	OutPathPoints.Reset();

	FVector ViewLocation;
	FRotator ViewRotation;
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar || !GetViewPoint(ViewLocation, ViewRotation))
	{
		return false;
	}

	const FVector Direction = ViewRotation.Vector();

	// 途中に壁・床があるか(あれば手前に置く。壁の向こうには置かない)
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GadgetPlace), false, Avatar);
	FHitResult Hit;
	const bool bHit = Avatar->GetWorld()->LineTraceSingleByChannel(
		Hit, ViewLocation, ViewLocation + Direction * MaxRange, ECC_Visibility, QueryParams);

	const FVector PlaceLocation = CalculatePlaceLocation(
		ViewLocation, Direction, MaxRange, bHit ? Hit.Distance : -1.0f, WallOffset);

	// 置く物は、プレイヤーから見て奥向きにする
	OutTransform = FTransform(FRotator(0.0f, ViewRotation.Yaw, 0.0f), PlaceLocation);

	// 空中でも置けるので、常に置ける
	return true;
}

float UGadgetPlaceAbility::GetPreviewRadius() const
{
	const ASlowField* SlowFieldDefaults = PlaceClass ? Cast<ASlowField>(PlaceClass->GetDefaultObject()) : nullptr;
	return SlowFieldDefaults ? SlowFieldDefaults->GetRadius() : 0.0f;
}

void UGadgetPlaceAbility::OnAimUpdated_Implementation(
	const FTransform& AimTransform,
	bool bIsValid,
	const TArray<FVector>& PathPoints)
{
	// 自分の画面だけで、狙っている間ずっと(毎フレーム)呼ばれる

	if (!bDrawDebugPreview)
	{
		return;
	}

	const AActor* Avatar = GetAvatarActorFromActorInfo();
	UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	const FColor Color = bIsValid ? FColor::Cyan : FColor::Red;
	const FVector PlaceLocation = AimTransform.GetLocation();

	// 中心(寿命 -1 = 1 フレームだけ表示。毎フレーム描き直す)
	DrawDebugSphere(World, PlaceLocation, 10.0f, 8, Color);

	const float Radius = GetPreviewRadius();
	if (Radius > 0.0f)
	{
		DrawDebugSphere(World, PlaceLocation, Radius, 16, Color, false, -1.0f, 0, 0.5f);
	}
}

void UGadgetPlaceAbility::OnConfirmed_Implementation(const FTransform& TargetTransform)
{
	// サーバーだけで呼ばれる
	// (クライアントから届いた位置は、親クラスが距離を確認済み)

	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!PlaceClass || !Avatar)
	{
		return;
	}

	FActorSpawnParameters SpawnParameters;
	// Owner: 置いた人
	SpawnParameters.Owner = Avatar;
	// Instigator: 置いた人(チーム判定に使う)
	SpawnParameters.Instigator = Cast<APawn>(Avatar);
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	Avatar->GetWorld()->SpawnActor<AActor>(PlaceClass, TargetTransform, SpawnParameters);
}

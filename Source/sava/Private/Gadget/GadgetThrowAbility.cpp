#include "Gadget/GadgetThrowAbility.h"

#include "Gadget/GadgetBase.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "SavaGameplayTags.h"

UGadgetThrowAbility::UGadgetThrowAbility()
{
	// 放物線で狙う(親クラスが予測線の点を計算して OnAimUpdated に渡してくれる)
	AimMode = ESavaAimMode::ProjectileArc;

	// 所持数。Commit Ability(= 投げた瞬間)に 1 減る
	MaxCharges = 2;

	// 狙っている間に右クリックでキャンセル
	CancelInputTag = SavaGameplayTags::InputTag_Weapon_Aim;

	// 能力の種類
	FGameplayTagContainer Tags = GetAssetTags();
	Tags.AddTag(SavaGameplayTags::Ability_Type_Gadget);
	SetAssetTags(Tags);
}

void UGadgetThrowAbility::OnAimStarted_Implementation()
{
	SyncAimSettingsFromGadget();
}

void UGadgetThrowAbility::SyncAimSettingsFromGadget()
{
	if (!GadgetClass)
	{
		return;
	}

	// CDO(クラスの既定値のオブジェクト)から設定を読む。
	// Spawn していない物の値を知りたいときに使う(Unity の Prefab の値を読むのに近い)
	const AGadgetBase* GadgetDefaults = GadgetClass->GetDefaultObject<AGadgetBase>();

	// 予測線と実際の軌道がずれないように、同じ速さ・太さで計算する
	LaunchSpeed = GadgetDefaults->GetGadgetData().ThrowSpeed;
	ProjectileRadius = GadgetDefaults->GetCollisionRadius();

	// 予測線はワールドの重力(倍率 1)で計算するので、倍率を変えると線がずれる
	if (!FMath::IsNearlyEqual(GadgetDefaults->GetGadgetData().Gravity, 1.0f))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("%s: Gadget Data の Gravity が 1 ではないため、予測線と実際の軌道がずれます"),
			*GetNameSafe(GadgetClass));
	}
}

void UGadgetThrowAbility::OnAimUpdated_Implementation(
	const FTransform& AimTransform,
	bool bIsValid,
	const TArray<FVector>& PathPoints)
{
	// 自分の画面だけで、狙っている間ずっと(毎フレーム)呼ばれる

	if (!bDrawDebugPreview || PathPoints.Num() < 2)
	{
		return;
	}

	const AActor* Avatar = GetAvatarActorFromActorInfo();
	UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	const FColor Color = bIsValid ? FColor::Green : FColor::Red;

	// 予測線(寿命 -1 = 1 フレームだけ表示。毎フレーム描き直す)
	for (int32 Index = 1; Index < PathPoints.Num(); ++Index)
	{
		DrawDebugLine(World, PathPoints[Index - 1], PathPoints[Index], Color, false, -1.0f, 0, 1.5f);
	}

	// 着弾点(予測線の最後の点)と、爆発の届く範囲
	const FVector ImpactPoint = PathPoints.Last();
	DrawDebugSphere(World, ImpactPoint, 10.0f, 8, Color);

	if (GadgetClass)
	{
		const float EffectRadius = GadgetClass->GetDefaultObject<AGadgetBase>()->GetGadgetData().EffectRadius;
		if (EffectRadius > 0.0f)
		{
			// 地面に平行な円(X 軸と Y 軸の面)
			DrawDebugCircle(World, ImpactPoint, EffectRadius, 32, Color, false, -1.0f, 0, 1.0f,
				FVector::XAxisVector, FVector::YAxisVector, false);
		}
	}
}

void UGadgetThrowAbility::OnConfirmed_Implementation(const FTransform& TargetTransform)
{
	// サーバーだけで呼ばれる。TargetTransform は「投げる位置と向き」
	// (クライアントから届いた値は、親クラスが距離などを確認済み)

	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!GadgetClass || !Avatar)
	{
		return;
	}

	FActorSpawnParameters SpawnParameters;
	// Owner: 投げた人(本人に当たらない・本人へのダメージ判定・味方判定に使う)
	SpawnParameters.Owner = Avatar;
	// Instigator: ダメージの原因になった人(チーム判定に使う)
	SpawnParameters.Instigator = Cast<APawn>(Avatar);
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AGadgetBase* Gadget = Avatar->GetWorld()->SpawnActor<AGadgetBase>(GadgetClass, TargetTransform, SpawnParameters);
	if (!Gadget)
	{
		return;
	}

	Gadget->ThrowGadget(TargetTransform.GetRotation().Vector());
}

#include "Gadget/FragGrenade.h"

#include "AbilitySystem/SavaAbilitySystemLibrary.h"
#include "Components/SphereComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"

AFragGrenade::AFragGrenade()
{
	//========================================
	// 初期値(Blueprint の子クラスで変更できる)
	//========================================

	m_GadgetData.GadgetId = TEXT("FragGrenade");
	m_GadgetData.ThrowSpeed = 1500.0f;
	m_GadgetData.Gravity = 1.0f;
	m_GadgetData.Damage = 100.0f;
	m_GadgetData.EffectRadius = 400.0f;

	// 球の当たり判定の初期値(32cm)は手投げの物には大きすぎるので小さくする
	m_CollisionComponent->InitSphereRadius(8.0f);
}

float AFragGrenade::CalculateFalloffDamage(
	float Distance,
	float MaxDamage,
	float Radius,
	float FullDamageRadius,
	float MinDamageRatio)
{
	if (Radius <= 0.0f || Distance > Radius)
	{
		return 0.0f;
	}

	if (Distance <= FullDamageRadius)
	{
		return MaxDamage;
	}

	// FullDamageRadius で 0、Radius で 1 になる割合
	// (ここに来るのは FullDamageRadius < Distance <= Radius のときなので 0 除算にならない)
	const float Alpha =
		(Distance - FullDamageRadius) / (Radius - FullDamageRadius);

	return FMath::Lerp(MaxDamage, MaxDamage * MinDamageRatio, Alpha);
}

void AFragGrenade::OnGadgetLanded(const FHitResult& Hit)
{
	// グレネードは着弾した瞬間に爆発する
	Detonate();
}

void AFragGrenade::ApplyGadgetEffect()
{
	// AGadgetBase::Detonate から呼ばれる(サーバーのみ)

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Origin = GetActorLocation();
	const float Radius = m_GadgetData.EffectRadius;
	const float MaxDamage = m_GadgetData.Damage;

	//========================================
	// 見た目・音(全員の画面)
	//========================================

	Multicast_PlayExplosion(Origin);

	if (Radius <= 0.0f || MaxDamage <= 0.0f)
	{
		return;
	}

	//========================================
	// 1. 範囲内のキャラクターを集める
	//========================================

	// Pawn(キャラクターのカプセルなど)だけを球で探す
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FragGrenade), false, this);
	World->OverlapMultiByObjectType(
		Overlaps,
		Origin,
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(Radius),
		QueryParams);

	// 1 人が複数の当たり判定を持つことがあるので、人ごとに一番近い距離を 1 つにまとめる
	// (まとめないと、同じ人に 2 回ダメージが入る)
	TMap<AActor*, float> ClosestDistances;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		UPrimitiveComponent* Component = Overlap.GetComponent();
		if (!Target || !Component)
		{
			continue;
		}

		// 相手の中心ではなく「当たり判定の表面で一番近い点」までの距離を使う
		// (中心だと、足元で爆発しても 90cm 離れている扱いになる)
		FVector ClosestPoint;
		float Distance = Component->GetClosestPointOnCollision(Origin, ClosestPoint);
		if (Distance < 0.0f)
		{
			// 取得できない当たり判定だった場合は中心までの距離
			Distance = FVector::Dist(Origin, Target->GetActorLocation());
		}

		const float* Existing = ClosestDistances.Find(Target);
		if (!Existing || Distance < *Existing)
		{
			ClosestDistances.Add(Target, Distance);
		}
	}

	//========================================
	// 2. 1 人ずつダメージを与える
	//========================================

	AActor* DamageInstigator = GetGadgetOwner();

	for (const TPair<AActor*, float>& Pair : ClosestDistances)
	{
		AActor* Target = Pair.Key;
		const float Distance = Pair.Value;

		if (!CanDamageTarget(Target))
		{
			continue;
		}

		if (m_bBlockedByWalls && IsBlockedByWall(Origin, Target))
		{
			continue;
		}

		const float Damage = CalculateFalloffDamage(
			Distance, MaxDamage, Radius, m_FullDamageRadius, m_MinDamageRatio);

		// ダメージは必ずこの関数を通す(GAS の GameplayEffect で HP を減らす)
		// 相手が ASC を持っていない(ダメージを受けない物)なら何もしない
		USavaAbilitySystemLibrary::ApplyDamage(DamageInstigator, Target, Damage, this);

		if (m_bDrawDebug)
		{
			DrawDebugString(World, Target->GetActorLocation(),
				FString::Printf(TEXT("%.0f (%.0fcm)"), Damage, Distance),
				nullptr, FColor::White, 3.0f);
		}
	}

	if (m_bDrawDebug)
	{
		DrawDebugSphere(World, Origin, Radius, 24, FColor::Red, false, 3.0f);
		DrawDebugSphere(World, Origin, m_FullDamageRadius, 16, FColor::Yellow, false, 3.0f);
	}
}

bool AFragGrenade::CanDamageTarget(const AActor* Target) const
{
	const AActor* GadgetOwner = GetGadgetOwner();

	// 投げた人がいない(退出した など)場合は全員にダメージ
	if (!GadgetOwner)
	{
		return true;
	}

	if (Target == GadgetOwner)
	{
		return m_bDamageOwner;
	}

	// 味方にはダメージなし
	return USavaAbilitySystemLibrary::AreEnemies(GadgetOwner, Target);
}

bool AFragGrenade::IsBlockedByWall(const FVector& From, const AActor* Target) const
{
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FragGrenadeWall), false, this);
	QueryParams.AddIgnoredActor(Target);

	// 壁・床など動かない物(WorldStatic)だけを調べる
	// (人や他の投げ物は爆風を遮らない)
	return GetWorld()->LineTraceTestByObjectType(
		From,
		Target->GetActorLocation(),
		FCollisionObjectQueryParams(ECC_WorldStatic),
		QueryParams);
}

void AFragGrenade::Multicast_PlayExplosion_Implementation(
	const FVector_NetQuantize& ExplosionLocation)
{
	OnExploded(ExplosionLocation);
}

#include "Gadget/StunGrenade.h"

#include "AbilitySystem/SavaAbilitySystemLibrary.h"
#include "Components/SphereComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "TimerManager.h"


AStunGrenade::AStunGrenade()
{
	//========================================
	// 初期値
	//========================================

	m_GadgetData.GadgetId = TEXT("StunGrenade");

	m_GadgetData.ThrowSpeed = 1500.0f;

	m_GadgetData.Gravity = 1.0f;

	// 着地してから起爆するまでの時間。
	// 実際のゲームバランスはBlueprint側で調整する。
	m_GadgetData.FuseTime = 0.0f;

	// スタン効果範囲。
	// 具体的な数値はゲームデザイン確定後に調整する。
	m_GadgetData.EffectRadius = 400.0f;

	// ダメージは与えない。
	m_GadgetData.Damage = 0.0f;

	// スタンの持続時間は現在未確定。
	m_StunDuration = 0.0f;

	// 球の当たり判定。
	m_CollisionComponent->InitSphereRadius(8.0f);
}


void AStunGrenade::OnGadgetLanded(
	const FHitResult& Hit)
{
	//========================================
	// Authority
	//========================================

	if (!HasAuthority())
	{
		return;
	}

	//========================================
	// Fuse
	//========================================

	const float FuseTime = m_GadgetData.FuseTime;

	/*
	 * FuseTimeが0以下なら着地した瞬間に起爆する。
	 */
	if (FuseTime <= 0.0f)
	{
		Detonate();
		return;
	}

	/*
	 * 着地してから一定時間待って起爆する。
	 *
	 * ゲームプレイ上重要な処理なので、サーバーだけで
	 * タイマーを動かす。
	 */
	GetWorldTimerManager().SetTimer(
		m_DetonationTimerHandle,
		this,
		&AStunGrenade::Detonate,
		FuseTime,
		false);
}


void AStunGrenade::ApplyGadgetEffect()
{
	//========================================
	// Authority
	//========================================

	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Origin = GetActorLocation();

	const float Radius =
		m_GadgetData.EffectRadius;

	//========================================
	// 視覚効果
	//========================================

	/*
	 * 起爆の視覚効果だけ全クライアントへ通知する。
	 *
	 * 実際のスタン判定はこの下でサーバーが行う。
	 */
	Multicast_PlayStunExplosion(Origin);

	if (Radius <= 0.0f)
	{
		return;
	}

	//========================================
	// 範囲内のキャラクターを取得
	//========================================

	TArray<FOverlapResult> Overlaps;

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(StunGrenade),
		false,
		this);

	World->OverlapMultiByObjectType(
		Overlaps,
		Origin,
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(Radius),
		QueryParams);

	/*
	 * 1人のキャラクターが複数のCollisionを持っている
	 * 場合があるため、Actor単位にまとめる。
	 */
	TSet<AActor*> Targets;

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();

		if (!Target)
		{
			continue;
		}

		Targets.Add(Target);
	}

	//========================================
	// スタン効果を適用
	//========================================

	for (AActor* Target : Targets)
	{
		if (!CanAffectTarget(Target))
		{
			continue;
		}

		//========================================
		// 壁判定
		//========================================

		if (m_bBlockedByWalls &&
			IsBlockedByWall(Origin, Target))
		{
			continue;
		}

		/*
		 * スタン状態の適用。
		 *
		 * ここではプレイヤーの移動速度やカメラを直接変更しない。
		 *
		 * GAS側のStatus Effect / GameplayEffectを通して
		 * 「スタン状態」を対象へ渡す構成にする。
		 *
		 * ※ 現時点ではプロジェクト側に
		 * スタン専用のApply関数が提示されていないため、
		 * 存在しないAPIを勝手に呼び出さない。
		 */

		 /*
		  * TODO:
		  *
		  * USavaAbilitySystemLibrary::ApplyStun(...)
		  *
		  * など、既存のGASスタン適用処理が完成した段階で
		  * ここへ接続する。
		  */

		if (m_bDrawDebug)
		{
			DrawDebugString(
				World,
				Target->GetActorLocation(),
				TEXT("STUN"),
				nullptr,
				FColor::White,
				3.0f);
		}
	}

	//========================================
	// Debug
	//========================================

	if (m_bDrawDebug)
	{
		DrawDebugSphere(
			World,
			Origin,
			Radius,
			24,
			FColor::Yellow,
			false,
			3.0f);
	}
}


bool AStunGrenade::CanAffectTarget(
	const AActor* Target) const
{
	if (!Target)
	{
		return false;
	}

	const AActor* GadgetOwner =
		GetGadgetOwner();

	/*
	 * Ownerが存在しない場合。
	 *
	 * 通常のGameplayではOwnerが設定される想定だが、
	 * Ownerが存在しない場合は対象として扱う。
	 */
	if (!GadgetOwner)
	{
		return true;
	}

	//========================================
	// 自分
	//========================================

	if (Target == GadgetOwner)
	{
		return m_bAffectOwner;
	}

	//========================================
	// 味方 / 敵
	//========================================

	if (USavaAbilitySystemLibrary::AreEnemies(
		GadgetOwner,
		Target))
	{
		// 敵
		return true;
	}

	// 味方
	return m_bAffectAllies;
}


bool AStunGrenade::IsBlockedByWall(
	const FVector& From,
	const AActor* Target) const
{
	if (!Target)
	{
		return true;
	}

	UWorld* World = GetWorld();

	if (!World)
	{
		return true;
	}

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(StunGrenadeWall),
		false,
		this);

	/*
	 * 対象自身は壁判定から除外する。
	 */
	QueryParams.AddIgnoredActor(Target);

	/*
	 * WorldStaticだけを調べる。
	 *
	 * プレイヤーや他のガジェットは
	 * スタン範囲を遮らない。
	 */
	return World->LineTraceTestByObjectType(
		From,
		Target->GetActorLocation(),
		FCollisionObjectQueryParams(ECC_WorldStatic),
		QueryParams);
}


void AStunGrenade::Multicast_PlayStunExplosion_Implementation(
	const FVector_NetQuantize& ExplosionLocation)
{
	/*
	 * Blueprint側で実際の視覚エフェクトを実装する。
	 *
	 * 例:
	 *
	 * Spawn System at Location
	 * Play Sound at Location ← 音は後で追加
	 */
	OnStunExploded(ExplosionLocation);
}
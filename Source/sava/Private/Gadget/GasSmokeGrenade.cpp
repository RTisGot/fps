#include "Gadget/GasSmokeGrenade.h"

#include "Gadget/GasSmokeArea.h"

AGasSmokeGrenade::AGasSmokeGrenade()
{
	m_GadgetData.GadgetId = TEXT("GasSmokeGrenade");

	// FragGrenadeと同じ投擲設定
	m_GadgetData.ThrowSpeed = 1500.0f;
	m_GadgetData.Gravity = 1.0f;

	// FragGrenadeと同じ効果範囲
	m_GadgetData.EffectRadius = 400.0f;

	// 効果時間は未確定のため、
	// Blueprint側でm_GadgetData.EffectDurationを設定する。
	m_GadgetData.EffectDuration = 0.0f;

	// GasSmokeArea側でダメージを処理するため、
	// GadgetData.Damageは使用しない。
	m_GadgetData.Damage = 0.0f;

	if (m_CollisionComponent)
	{
		m_CollisionComponent->SetSphereRadius(8.0f);
	}
}

void AGasSmokeGrenade::OnGadgetLanded(const FHitResult& Hit)
{
	Super::OnGadgetLanded(Hit);

	if (!HasAuthority())
	{
		return;
	}

	// 着地したら即座に起爆。
	Detonate();
}

void AGasSmokeGrenade::ApplyGadgetEffect()
{
	if (!HasAuthority())
	{
		return;
	}

	if (!m_GasSmokeAreaClass)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("AGasSmokeGrenade::ApplyGadgetEffect: GasSmokeAreaClass is not set."));
		return;
	}

	UWorld* World = GetWorld();

	if (!World)
	{
		return;
	}

	const FVector SmokeLocation = GetActorLocation();
	const FRotator SmokeRotation = FRotator::ZeroRotator;

	/*
	 * Deferred Spawnにすることで、
	 * BeginPlay前にGasSmokeAreaへ設定値を渡す。
	 */
	AGasSmokeArea* GasSmokeArea =
		World->SpawnActorDeferred<AGasSmokeArea>(
			m_GasSmokeAreaClass,
			FTransform(
				SmokeRotation,
				SmokeLocation),
			GetGadgetOwner(),
			GetInstigator(),
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (!GasSmokeArea)
	{
		return;
	}

	GasSmokeArea->InitializeGas(
		GetGadgetOwner(),
		m_GadgetData.EffectRadius,
		m_GadgetData.EffectDuration,
		m_DamagePerSecond);

	GasSmokeArea->FinishSpawning(
		FTransform(
			SmokeRotation,
			SmokeLocation));

	// 演出はGrenade本体ではなく、
	// 起爆位置を渡してBlueprint側で再生する。
	OnGasSmokeStarted(SmokeLocation);
}
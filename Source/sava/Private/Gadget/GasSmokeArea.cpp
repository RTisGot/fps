#include "Gadget/GasSmokeArea.h"

#include "AbilitySystem/SavaAbilitySystemLibrary.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

AGasSmokeArea::AGasSmokeArea()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;

	m_EffectCollisionComponent =
		CreateDefaultSubobject<USphereComponent>(
			TEXT("EffectCollision"));

	RootComponent = m_EffectCollisionComponent;

	m_EffectCollisionComponent->SetSphereRadius(400.0f);

	m_EffectCollisionComponent->SetCollisionEnabled(
		ECollisionEnabled::QueryOnly);

	m_EffectCollisionComponent->SetCollisionResponseToAllChannels(
		ECR_Ignore);

	m_EffectCollisionComponent->SetCollisionResponseToChannel(
		ECC_Pawn,
		ECR_Overlap);
}

void AGasSmokeArea::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	OnGasSmokeStarted();

	DrawDebugSphere(
		GetWorld(),
		GetActorLocation(),
		m_EffectRadius,
		32,
		FColor::Green,
		false,
		m_EffectDuration > 0.0f
		? m_EffectDuration
		: 5.0f,
		0,
		2.0f);

	// 生成直後にも1回ダメージを判定する。
	ApplyGasDamage();

	// 継続ダメージ。
	if (m_DamageInterval > 0.0f)
	{
		GetWorldTimerManager().SetTimer(
			m_DamageTimerHandle,
			this,
			&AGasSmokeArea::ApplyGasDamage,
			m_DamageInterval,
			true,
			m_DamageInterval);
	}

	// 効果時間が設定されている場合は終了タイマーを設定する。
	if (m_EffectDuration > 0.0f)
	{
		GetWorldTimerManager().SetTimer(
			m_EndTimerHandle,
			this,
			&AGasSmokeArea::EndGasSmoke,
			m_EffectDuration,
			false);
	}
}

void AGasSmokeArea::InitializeGas(
	AActor* InOwner,
	float InRadius,
	float InDuration,
	float InDamagePerSecond,
	bool bInAffectOwner)
{
	if (!HasAuthority())
	{
		return;
	}

	m_GasOwner = InOwner;

	m_EffectRadius = InRadius;
	m_EffectDuration = InDuration;
	m_DamagePerSecond = InDamagePerSecond;

	// 自分へのダメージ設定を受け取る。
	m_bAffectOwner = bInAffectOwner;

	m_EffectCollisionComponent->SetSphereRadius(
		m_EffectRadius);
}

void AGasSmokeArea::ApplyGasDamage()
{
	if (!HasAuthority())
	{
		return;
	}

	if (!IsValid(m_GasOwner))
	{
		return;
	}

	if (m_DamagePerSecond <= 0.0f ||
		m_DamageInterval <= 0.0f)
	{
		return;
	}

	TArray<AActor*> OverlappingActors;

	m_EffectCollisionComponent->GetOverlappingActors(
		OverlappingActors,
		APawn::StaticClass());

	const float Damage =
		m_DamagePerSecond * m_DamageInterval;

	for (AActor* Target : OverlappingActors)
	{
		if (!IsValid(Target))
		{
			continue;
		}

		if (!CanDamageTarget(Target))
		{
			continue;
		}

		USavaAbilitySystemLibrary::ApplyDamage(
			m_GasOwner,
			Target,
			Damage,
			this);
	}
}

bool AGasSmokeArea::CanDamageTarget(
	const AActor* Target) const
{
	if (!IsValid(Target))
	{
		return false;
	}

	if (!IsValid(m_GasOwner))
	{
		return true;
	}

	// 自分自身。
	if (Target == m_GasOwner)
	{
		return m_bAffectOwner;
	}

	// 敵にはダメージを与える。
	if (USavaAbilitySystemLibrary::AreEnemies(
		m_GasOwner,
		Target))
	{
		return true;
	}

	// 味方にはダメージを与えない。
	return false;
}

void AGasSmokeArea::EndGasSmoke()
{
	if (!HasAuthority())
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(
		m_DamageTimerHandle);

	OnGasSmokeEnded();

	Destroy();
}
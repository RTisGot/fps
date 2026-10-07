#include "Gadget/GasSmokeArea.h"

#include "AbilitySystem/SavaAbilitySystemLibrary.h"
#include "Components/SphereComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

AGasSmokeArea::AGasSmokeArea()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;

	//========================================
	// 範囲コリジョン
	//========================================

	m_EffectCollisionComponent =
		CreateDefaultSubobject<USphereComponent>(TEXT("EffectCollision"));

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

	//========================================
	// ガスのゲーム処理はサーバーのみ
	//========================================

	if (!HasAuthority())
	{
		return;
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("GasSmokeArea BeginPlay: Owner=%s Radius=%.1f Duration=%.1f DPS=%.1f"),
		*GetNameSafe(m_GasOwner),
		m_EffectRadius,
		m_EffectDuration,
		m_DamagePerSecond);

	//========================================
	// ガス煙開始演出
	//========================================

	OnGasSmokeStarted();

	//========================================
	// デバッグ表示
	//========================================

	DrawDebugSphere(
		GetWorld(),
		GetActorLocation(),
		m_EffectRadius,
		32,
		FColor::Green,
		false,
		m_EffectDuration > 0.0f ? m_EffectDuration : 5.0f,
		0,
		2.0f);

	//========================================
	// 初回ダメージ
	//========================================

	ApplyGasDamage();

	//========================================
	// 継続ダメージ
	//========================================

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

	//========================================
	// ガス終了タイマー
	//========================================

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
	float InDamagePerSecond)
{
	if (!HasAuthority())
	{
		return;
	}

	//========================================
	// 初期値設定
	//========================================

	m_GasOwner = InOwner;

	m_EffectRadius = InRadius;
	m_EffectDuration = InDuration;
	m_DamagePerSecond = InDamagePerSecond;

	//========================================
	// 範囲コリジョン設定
	//========================================

	m_EffectCollisionComponent->SetSphereRadius(
		m_EffectRadius);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("GasSmokeArea InitializeGas: Owner=%s Radius=%.1f Duration=%.1f DPS=%.1f"),
		*GetNameSafe(m_GasOwner),
		m_EffectRadius,
		m_EffectDuration,
		m_DamagePerSecond);
}

void AGasSmokeArea::ApplyGasDamage()
{
	if (!HasAuthority())
	{
		return;
	}

	if (!IsValid(m_GasOwner))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("GasSmokeArea ApplyGasDamage: GasOwner is invalid."));

		return;
	}

	if (m_DamagePerSecond <= 0.0f ||
		m_DamageInterval <= 0.0f)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("GasSmokeArea ApplyGasDamage: Invalid damage settings. DPS=%.1f Interval=%.1f"),
			m_DamagePerSecond,
			m_DamageInterval);

		return;
	}

	//========================================
	// 範囲内のPawnを取得
	//========================================

	TArray<AActor*> OverlappingActors;

	m_EffectCollisionComponent->GetOverlappingActors(
		OverlappingActors,
		APawn::StaticClass());

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("GasSmokeArea ApplyGasDamage: OverlappingActors=%d"),
		OverlappingActors.Num());

	//========================================
	// この1回で与えるダメージ
	//========================================

	const float Damage =
		m_DamagePerSecond * m_DamageInterval;

	//========================================
	// 対象ごとにダメージ
	//========================================

	for (AActor* Target : OverlappingActors)
	{
		if (!IsValid(Target))
		{
			continue;
		}

		//========================================
		// ダメージ対象判定
		//========================================

		if (!CanDamageTarget(Target))
		{
			continue;
		}

		UE_LOG(
			LogTemp,
			Warning,
			TEXT("GasSmokeArea Damage: Target=%s Damage=%.1f"),
			*GetNameSafe(Target),
			Damage);

		//========================================
		// GAS経由でダメージ
		//========================================

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

	//========================================
	// Ownerが存在しない場合
	//========================================

	if (!IsValid(m_GasOwner))
	{
		return true;
	}

	//========================================
	// 自分自身
	//========================================

	if (Target == m_GasOwner)
	{
		return m_bAffectOwner;
	}

	//========================================
	// 敵
	//========================================

	if (USavaAbilitySystemLibrary::AreEnemies(
		m_GasOwner,
		Target))
	{
		return true;
	}

	//========================================
	// 味方
	//========================================

	return m_bAffectAllies;
}

void AGasSmokeArea::EndGasSmoke()
{
	if (!HasAuthority())
	{
		return;
	}

	//========================================
	// ダメージタイマー停止
	//========================================

	GetWorldTimerManager().ClearTimer(
		m_DamageTimerHandle);

	//========================================
	// 終了演出
	//========================================

	OnGasSmokeEnded();

	//========================================
	// ガスエリア削除
	//========================================

	Destroy();
}
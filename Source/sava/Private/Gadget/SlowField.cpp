#include "Gadget/SlowField.h"

#include "AbilitySystem/SavaAbilitySystemLibrary.h"
#include "AbilitySystem/SavaGameplayEffects.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/SphereComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "SavaGameplayTags.h"
#include "TimerManager.h"

ASlowField::ASlowField()
{
	PrimaryActorTick.bCanEverTick = false;

	// Spawn・消去はサーバーが行い、全員へ同期する(置いた後は動かない)
	bReplicates = true;

	m_AreaComponent = CreateDefaultSubobject<USphereComponent>(TEXT("Area"));
	RootComponent = m_AreaComponent;

	// 範囲の判定は Overlap の問い合わせで行うので、当たり判定は持たない
	m_AreaComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	m_AreaComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	m_AreaComponent->InitSphereRadius(m_Radius);

	m_SlowEffect = USavaGE_Slowed::StaticClass();
}

void ASlowField::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Blueprint で Radius を変えたら表示用の球も合わせる
	m_AreaComponent->SetSphereRadius(m_Radius);
}

void ASlowField::BeginPlay()
{
	Super::BeginPlay();

	// 見た目(全員の画面)
	OnSlowFieldStarted();

	if (m_bDrawDebug)
	{
		DrawDebugSphere(GetWorld(), GetActorLocation(), m_Radius, 24, FColor::Cyan, false,
			m_Duration > 0.0f ? m_Duration : 5.0f, 0, 1.0f);
	}

	if (!HasAuthority())
	{
		return;
	}

	m_OwnerPlayerState = USavaAbilitySystemLibrary::GetOwningPlayerState(this);

	if (m_Duration > 0.0f)
	{
		// 時間が来たら Destroy される(EndPlay で減速を外す)
		SetLifeSpan(m_Duration);
	}

	// 置いた瞬間に中にいた人にもすぐ効かせる
	RefreshTargets();

	GetWorldTimerManager().SetTimer(
		m_RefreshTimerHandle,
		this,
		&ASlowField::RefreshTargets,
		m_RefreshInterval,
		true);
}

void ASlowField::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		GetWorldTimerManager().ClearTimer(m_RefreshTimerHandle);
		RemoveAllSlows();
	}

	OnSlowFieldEnded();

	Super::EndPlay(EndPlayReason);
}

void ASlowField::RefreshTargets()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	//========================================
	// 1. 範囲内で減速させる相手(の ASC)を集める
	//========================================

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SlowField), false, this);
	World->OverlapMultiByObjectType(
		Overlaps,
		GetActorLocation(),
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(m_Radius),
		QueryParams);

	// 1 人が複数の当たり判定を持つことがあるので、ASC ごとにまとめる
	TSet<UAbilitySystemComponent*> TargetsInside;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		if (!Target || !CanAffectTarget(Target))
		{
			continue;
		}

		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
		if (!TargetASC || TargetASC->HasMatchingGameplayTag(SavaGameplayTags::State_Dead))
		{
			continue;
		}

		TargetsInside.Add(TargetASC);
	}

	//========================================
	// 2. 出た人(と、いなくなった人)から外す
	//========================================

	for (auto It = m_SlowedTargets.CreateIterator(); It; ++It)
	{
		UAbilitySystemComponent* TargetASC = It.Key().Get();
		if (TargetASC && TargetsInside.Contains(TargetASC))
		{
			continue;
		}

		if (TargetASC)
		{
			TargetASC->RemoveActiveGameplayEffect(It.Value());
		}
		It.RemoveCurrent();
	}

	//========================================
	// 3. 入った人に付ける
	//========================================

	for (UAbilitySystemComponent* TargetASC : TargetsInside)
	{
		if (m_SlowedTargets.Contains(TargetASC))
		{
			continue;
		}

		// 他のスローエリアで既に遅くなっている人には重ねない
		// (そちらが消えたら、次の調べ直しでこちらが付ける)
		if (TargetASC->HasMatchingGameplayTag(SavaGameplayTags::State_Slowed))
		{
			continue;
		}

		ApplySlow(TargetASC);
	}
}

void ASlowField::ApplySlow(UAbilitySystemComponent* TargetASC)
{
	if (!m_SlowEffect)
	{
		return;
	}

	// 置いた人の ASC で効果を作る(誰の効果か分かるように)。いなければ相手側で作る
	UAbilitySystemComponent* SourceASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(m_OwnerPlayerState.Get());
	UAbilitySystemComponent* SpecOwnerASC = SourceASC ? SourceASC : TargetASC;

	FGameplayEffectContextHandle Context = SpecOwnerASC->MakeEffectContext();
	Context.AddInstigator(GetInstigator(), this);

	const FGameplayEffectSpecHandle SpecHandle = SpecOwnerASC->MakeOutgoingSpec(m_SlowEffect, 1.0f, Context);
	if (!SpecHandle.IsValid())
	{
		return;
	}
	SpecHandle.Data->SetSetByCallerMagnitude(SavaGameplayTags::SetByCaller_MoveSpeedMultiplier, m_MoveSpeedMultiplier);

	const FActiveGameplayEffectHandle EffectHandle =
		SpecOwnerASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
	if (EffectHandle.IsValid())
	{
		m_SlowedTargets.Add(TargetASC, EffectHandle);
	}
}

void ASlowField::RemoveAllSlows()
{
	for (const TPair<TWeakObjectPtr<UAbilitySystemComponent>, FActiveGameplayEffectHandle>& Pair : m_SlowedTargets)
	{
		if (UAbilitySystemComponent* TargetASC = Pair.Key.Get())
		{
			TargetASC->RemoveActiveGameplayEffect(Pair.Value);
		}
	}
	m_SlowedTargets.Reset();
}

bool ASlowField::CanAffectTarget(const AActor* Target) const
{
	const APlayerState* OwnerPlayerState = m_OwnerPlayerState.Get();

	// 置いた人がいない(退出した など)場合は全員に効く
	if (!OwnerPlayerState)
	{
		return true;
	}

	if (USavaAbilitySystemLibrary::GetOwningPlayerState(Target) == OwnerPlayerState)
	{
		return m_bAffectOwner;
	}

	return USavaAbilitySystemLibrary::AreEnemies(OwnerPlayerState, Target) || m_bAffectAllies;
}

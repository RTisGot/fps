// Fill out your copyright notice in the Description page of Project Settings.

#include "Weapon/SavaWeaponFireAbility.h"
#include "Weapon/SavaCombatTestDummyCharacter.h"
#include "Weapon/SavaEquipmentComponent.h"
#include "Weapon/SavaWeaponData.h"
#include "Skill/SavaDomeShield.h"
#include "AbilitySystem/SavaAbilitySystemLibrary.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "SavaGameplayTags.h"

DEFINE_LOG_CATEGORY_STATIC(LogSavaWeapon, Log, All);

namespace
{
	//頭に当たった弾の目印(FHitResult の BoneName に入れてサーバーへ送る)
	const FName HeadBoneName(TEXT("head"));

	//当たり判定の確認用の表示(コンソールで sava.Weapon.Debug 1)
	TAutoConsoleVariable<bool> CVarWeaponDebug(
		TEXT("sava.Weapon.Debug"),
		false,
		TEXT("武器の当たり判定を表示する。線 = 自分の画面のレイ(黄: 胴体 / 赤: 頭 / 白: 外れ)、球 = サーバーの結果(緑: ダメージ / 紫: 頭 / 水色: 物を押した / 灰: ダメージなし / 赤: 却下と理由)"));

	constexpr float DebugDrawTime = 3.0f;

	bool IsWeaponDebugEnabled()
	{
		return CVarWeaponDebug.GetValueOnGameThread();
	}

	//サーバーの判定結果を、当たった位置に表示してログにも出す
	void DrawServerResult(const UWorld* World, const FHitResult& Hit, const FColor& Color, const FString& Text)
	{
		if (!IsWeaponDebugEnabled())
		{
			return;
		}
		DrawDebugSphere(World, Hit.ImpactPoint, 10.0f, 8, Color, false, DebugDrawTime);
		DrawDebugString(World, Hit.ImpactPoint + FVector(0.0f, 0.0f, 20.0f), Text, nullptr, Color, DebugDrawTime);
		UE_LOG(LogSavaWeapon, Log, TEXT("[Server] %s -> %s"), *GetNameSafe(Hit.GetActor()), *Text);
	}
}

USavaWeaponFireAbility::USavaWeaponFireAbility()
{
	ActivationPolicy = ESavaAbilityActivationPolicy::OnInputTriggered;

	AddAssetTag(SavaGameplayTags::Ability_Weapon_Fire);
	ActivationBlockedTags.AddTag(SavaGameplayTags::State_Weapon_Reloading);
}

void USavaWeaponFireAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	//※ Super::ActivateAbility は呼ばない(Blueprint の Event ActivateAbility は使わない設計)

	USavaEquipmentComponent* Equipment = GetEquipmentFromActorInfo(ActorInfo);
	const USavaWeaponData* Weapon = Equipment ? Equipment->GetCurrentWeaponData() : nullptr;
	if (!Weapon || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	FiringSlot = Equipment->GetCurrentSlot();

	if (IsLocallyControlled())
	{
		//操作している本人: 撃つ(ホストの場合は、サーバーの確認もこの中で行う)
		if (Equipment->GetAmmoInMagazine(FiringSlot) <= 0)
		{
			EndAndReload();
			return;
		}

		const FSavaWeaponStats Stats = Equipment->GetStats(FiringSlot);
		switch (Weapon->FireMode)
		{
		case ESavaFireMode::SemiAuto: ShotsRemaining = 1; break;
		case ESavaFireMode::Burst:    ShotsRemaining = FMath::Max(Stats.BurstCount, 1); break;
		case ESavaFireMode::FullAuto: ShotsRemaining = -1; break;
		}

		//単発・バーストで、前の 1 発から間隔が空いていない: このクリックは無視する
		const float RemainingInterval = Equipment->GetRemainingFireInterval(FiringSlot);
		if (RemainingInterval > 0.0f && Weapon->FireMode != ESavaFireMode::FullAuto)
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
			return;
		}

		//フルオート: 離したら終わる
		if (Weapon->FireMode == ESavaFireMode::FullAuto)
		{
			UAbilityTask_WaitInputRelease* WaitReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
			WaitReleaseTask->OnRelease.AddDynamic(this, &USavaWeaponFireAbility::OnInputReleased);
			WaitReleaseTask->ReadyForActivation();
		}

		if (RemainingInterval <= 0.0f)
		{
			FireShot();
			if (!IsActive())
			{
				return;
			}
		}

		//2 発目以降(バースト・フルオート)は間隔ごとに撃つ
		const float Interval = Stats.GetFireInterval();
		GetWorld()->GetTimerManager().SetTimer(FireTimerHandle, this, &USavaWeaponFireAbility::FireShot, Interval, true,
			RemainingInterval > 0.0f ? RemainingInterval : Interval);
	}
	else if (ActorInfo->IsNetAuthority())
	{
		//サーバー(撃ったのがクライアントの場合): 1 回ごとに届く結果を待つ。終了はクライアントから伝わる
		UAbilitySystemComponent* AbilitySystem = ActorInfo->AbilitySystemComponent.Get();
		const FPredictionKey ActivationKey = ActivationInfo.GetActivationPredictionKey();
		AbilitySystem->AbilityTargetDataSetDelegate(Handle, ActivationKey).AddUObject(this, &USavaWeaponFireAbility::OnServerTargetDataReceived);
		bListeningForServerTargetData = true;

		//発動の通知より先に届いていた場合
		AbilitySystem->CallReplicatedTargetDataDelegatesIfSet(Handle, ActivationKey);
	}
}

void USavaWeaponFireAbility::FireShot()
{
	//死んだ・持ち替えたら止める
	USavaEquipmentComponent* Equipment = GetEquipmentComponent();
	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo();
	if (!Equipment || !AbilitySystem || AbilitySystem->HasMatchingGameplayTag(SavaGameplayTags::State_Dead)
		|| Equipment->GetCurrentSlot() != FiringSlot)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	//弾切れ
	if (!Equipment->TryConsumeLocalShot(FiringSlot))
	{
		EndAndReload();
		return;
	}

	const FSavaWeaponStats Stats = Equipment->GetStats(FiringSlot);
	TArray<FHitResult> Hits;
	TraceShot(Stats, Equipment->GetCurrentSpreadAngle(), Hits);

	//デバッグ用の弾道表示
	for (const FHitResult& Hit : Hits)
	{
		DrawDebugLine(
			GetWorld(),
			Hit.TraceStart,
			Hit.ImpactPoint,
			FColor::Red,
			false,
			1.0f,
			0,
			1.0f);
	}

	//武器の視覚演出を全プレイヤーへ通知する
	FVector MuzzleLocation;
	if (Equipment->GetCurrentMuzzleLocation(MuzzleLocation))
	{
		TArray<FVector> TraceEnds;
		TraceEnds.Reserve(Hits.Num());

		for (const FHitResult& Hit : Hits)
		{
			TraceEnds.Add(Hit.ImpactPoint);
		}

		Equipment->NotifyWeaponFireVisual(MuzzleLocation, TraceEnds);
	}

	//既存のローカル演出
	OnFired(Hits);

	//反動はレイを飛ばした後に付ける(次の 1 発から影響する)
	Equipment->ApplyRecoil(FiringSlot);

	if (IsPredictingClient())
	{
		//クライアント: 結果をサーバーへ送る(外れも送る。サーバーが弾数と撃つ間隔を数えるため)
		FScopedPredictionWindow ScopedPrediction(AbilitySystem, true);

		FGameplayAbilityTargetDataHandle DataHandle;
		for (const FHitResult& Hit : Hits)
		{
			DataHandle.Add(new FGameplayAbilityTargetData_SingleTargetHit(Hit));
		}
		AbilitySystem->ServerSetReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey(),
			DataHandle, FGameplayTag(), AbilitySystem->ScopedPredictionKey);
	}
	else
	{
		//サーバーで操作している本人(リッスンサーバーのホスト・一人プレイ)
		ProcessShotOnServer(Hits, false);
	}

	//単発・バーストは決まった回数撃ったら終わる
	if (ShotsRemaining > 0 && --ShotsRemaining == 0)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void USavaWeaponFireAbility::EndAndReload()
{
	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);

	if (AbilitySystem)
	{
		AbilitySystem->TryActivateAbilitiesByTag(FGameplayTagContainer(SavaGameplayTags::Ability_Weapon_Reload));
	}
}

void USavaWeaponFireAbility::OnServerTargetDataReceived(const FGameplayAbilityTargetDataHandle& DataHandle, FGameplayTag ApplicationTag)
{
	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo();
	AbilitySystem->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());

	TArray<FHitResult> Hits;
	for (int32 Index = 0; Index < DataHandle.Num(); ++Index)
	{
		const FGameplayAbilityTargetData* TargetData = DataHandle.Get(Index);
		if (TargetData && TargetData->HasHitResult())
		{
			Hits.Add(*TargetData->GetHitResult());
		}
	}

	if (Hits.Num() > 0)
	{
		ProcessShotOnServer(Hits, true);
	}
}

void USavaWeaponFireAbility::ProcessShotOnServer(const TArray<FHitResult>& Hits, bool bFromRemoteClient)
{
	USavaEquipmentComponent* Equipment = GetEquipmentComponent();
	if (!Equipment)
	{
		return;
	}

	//クライアントの弾: サーバーでも弾数を数え、弾切れ・撃つ間隔が短すぎる場合はダメージを与えない
	//(ホストは自分の画面で弾を使ったので、ここでは数えない)
	if (bFromRemoteClient && !Equipment->TryConsumeServerShot(FiringSlot, ServerFireIntervalTolerance))
	{
		for (const FHitResult& Hit : Hits)
		{
			DrawServerResult(GetWorld(), Hit, FColor::Red, TEXT("Rejected: out of ammo / firing too fast"));
		}
		return;
	}

	const FSavaWeaponStats Stats = Equipment->GetStats(FiringSlot);
	AActor* Avatar = GetAvatarActorFromActorInfo();

	//本来の弾の数より多く送られてきても、その分は使わない
	const int32 NumPellets = FMath::Min(Hits.Num(), FMath::Max(Stats.PelletCount, 1));
	for (int32 Index = 0; Index < NumPellets; ++Index)
	{
		const FHitResult& Hit = Hits[Index];
		AActor* HitActor = Hit.GetActor();
		if (!Hit.bBlockingHit || !HitActor)
		{
			continue;
		}

		//シールドに当たった弾はダメージなし
		if (HitActor->IsA<ASavaDomeShield>())
		{
			DrawServerResult(GetWorld(), Hit, FColor::Cyan, TEXT("Blocked by shield"));
			continue;
		}

		//クライアントから届いた結果は信用せず、ありえる当たりかを確認する
		if (bFromRemoteClient)
		{
			if (const TCHAR* RejectReason = GetHitRejectReason(Hit))
			{
				DrawServerResult(GetWorld(), Hit, FColor::Red, FString::Printf(TEXT("Rejected: %s"), RejectReason));
				continue;
			}
		}

		PushPhysicsObject(Hit);

		if (!USavaAbilitySystemLibrary::AreEnemies(Avatar, HitActor))
		{
			DrawServerResult(GetWorld(), Hit, FColor::Silver, TEXT("No damage (not an enemy)"));
			continue;
		}

		const bool bHeadshot = Hit.BoneName == HeadBoneName && (!bFromRemoteClient || IsHeadHitPlausible(Hit));
		const float Damage = Stats.CalculateDamage(FVector::Dist(Hit.TraceStart, Hit.ImpactPoint), bHeadshot);
		if (USavaAbilitySystemLibrary::ApplyDamage(Avatar, HitActor, Damage, Avatar))
		{
			if (ASavaCombatTestDummyCharacter* TestDummy = Cast<ASavaCombatTestDummyCharacter>(HitActor))
			{
				TestDummy->NotifyWeaponImpact(Hit, Damage, bHeadshot);
			}
			DrawServerResult(GetWorld(), Hit, bHeadshot ? FColor::Magenta : FColor::Green,
				FString::Printf(TEXT("%s %.1f"), bHeadshot ? TEXT("HEAD") : TEXT("Body"), Damage));
			OnHitConfirmed(Hit, Damage, bHeadshot);
		}
		else
		{
			//物理の箱など、HP を持たない物
			const UPrimitiveComponent* HitComponent = Hit.GetComponent();
			const bool bPushed = PhysicsImpulseSpeed > 0.0f && HitComponent && HitComponent->IsSimulatingPhysics();
			DrawServerResult(GetWorld(), Hit, bPushed ? FColor::Cyan : FColor::Silver, bPushed ? TEXT("Pushed") : TEXT("No damage (no health)"));
		}
	}
}

void USavaWeaponFireAbility::PushPhysicsObject(const FHitResult& Hit) const
{
	UPrimitiveComponent* HitComponent = Hit.GetComponent();
	if (PhysicsImpulseSpeed <= 0.0f || !HitComponent || !HitComponent->IsSimulatingPhysics())
	{
		return;
	}

	//重さに関係なく、同じ速さだけ弾の向きへ押す(当たった位置を押すので回転もする)
	const FVector Direction = (Hit.ImpactPoint - Hit.TraceStart).GetSafeNormal();
	HitComponent->AddImpulseAtLocation(Direction * PhysicsImpulseSpeed * HitComponent->GetMass(), Hit.ImpactPoint, Hit.BoneName);
}

const TCHAR* USavaWeaponFireAbility::GetHitRejectReason(const FHitResult& Hit) const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	const AActor* HitActor = Hit.GetActor();
	FVector ViewLocation;
	FRotator ViewRotation;
	if (!Avatar || !HitActor || !GetViewPoint(ViewLocation, ViewRotation))
	{
		return TEXT("no shooter / target");
	}

	//1. 撃った位置が、サーバーから見たプレイヤーの視点の近くか
	if (FVector::Dist(Hit.TraceStart, ViewLocation) > ServerViewTolerance)
	{
		return TEXT("shot origin too far from player's view");
	}

	//2. レイが届く距離か
	if (FVector::Dist(Hit.TraceStart, Hit.ImpactPoint) > MaxTraceDistance)
	{
		return TEXT("out of range");
	}

	//3. 当たった相手が、当たった位置の近くにいるか
	if (FVector::Dist(HitActor->GetActorLocation(), Hit.ImpactPoint) > ServerHitTolerance)
	{
		return TEXT("target is not near the hit point");
	}

	//4. 撃った位置から当たった位置までの間に壁がないか(壁抜きの防止)
	//   太い弾は当たった瞬間の弾の中心(Location)まで調べる
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SavaWeaponFireCheck), false, Avatar);
	QueryParams.AddIgnoredActor(HitActor);
	FHitResult BlockingHit;
	if (Avatar->GetWorld()->LineTraceSingleByChannel(BlockingHit, Hit.TraceStart, Hit.Location, ECC_Visibility, QueryParams))
	{
		return TEXT("blocked by a wall");
	}

	//5. 撃った位置から当たった位置までの間にドームシールドがないか
	FVector ShieldHitLocation;
	FVector ShieldHitNormal;
	if (ASavaDomeShield::FindBlockingShield(Avatar->GetWorld(), Hit.TraceStart, Hit.ImpactPoint, ShieldHitLocation, ShieldHitNormal))
	{
		return TEXT("blocked by a shield");
	}

	return nullptr;
}

bool USavaWeaponFireAbility::IsHeadHitPlausible(const FHitResult& Hit) const
{
	const ACharacter* HitCharacter = Cast<ACharacter>(Hit.GetActor());
	if (!HitCharacter)
	{
		return false;
	}

	//サーバーから見た頭の中心の近くに当たっているか
	const UCapsuleComponent* Capsule = HitCharacter->GetCapsuleComponent();
	const FVector HeadCenter = Capsule->GetComponentLocation() + FVector(0.0f, 0.0f, Capsule->GetScaledCapsuleHalfHeight() - HeadshotHeight * 0.5f);
	return FVector::Dist(Hit.ImpactPoint, HeadCenter) <= ServerHeadshotTolerance;
}

void USavaWeaponFireAbility::OnInputReleased(float TimeHeld)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void USavaWeaponFireAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FireTimerHandle);
	}

	if (bListeningForServerTargetData)
	{
		bListeningForServerTargetData = false;
		if (UAbilitySystemComponent* AbilitySystem = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
		{
			AbilitySystem->AbilityTargetDataSetDelegate(Handle, ActivationInfo.GetActivationPredictionKey()).RemoveAll(this);
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

//--------------------------------Trace

bool USavaWeaponFireAbility::GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	//サーバーでも、そのプレイヤーの視点(複製された向き)で計算できる
	const APawn* Pawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	const AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	if (!Controller)
	{
		return false;
	}
	Controller->GetPlayerViewPoint(OutLocation, OutRotation);
	return true;
}

void USavaWeaponFireAbility::TraceShot(const FSavaWeaponStats& Stats, float SpreadAngle, TArray<FHitResult>& OutHits) const
{
	FVector ViewLocation;
	FRotator ViewRotation;
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar || !GetViewPoint(ViewLocation, ViewRotation))
	{
		return;
	}

	const UWorld* World = Avatar->GetWorld();
	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SavaWeaponFire), false, Avatar);
	const FVector AimDirection = ViewRotation.Vector();
	const float SpreadHalfAngle = FMath::DegreesToRadians(SpreadAngle);

	for (int32 Pellet = 0; Pellet < FMath::Max(Stats.PelletCount, 1); ++Pellet)
	{
		//拡散: 狙った方向から最大 Spread Angle だけランダムにずらす
		const FVector Direction = SpreadHalfAngle > 0.0f ? FMath::VRandCone(AimDirection, SpreadHalfAngle) : AimDirection;
		const FVector TraceEnd = ViewLocation + Direction * MaxTraceDistance;

		FHitResult Hit;
		const bool bHit = Stats.HitScanRadius > 0.0f
			? World->SweepSingleByChannel(Hit, ViewLocation, TraceEnd, FQuat::Identity, TraceChannel, FCollisionShape::MakeSphere(Stats.HitScanRadius), QueryParams)
			: World->LineTraceSingleByChannel(Hit, ViewLocation, TraceEnd, TraceChannel, QueryParams);

		//ドームシールドを横切るなら、そこで弾を止める(シールドには当たり判定が無いので計算で調べる)
		FVector ShieldHitLocation;
		FVector ShieldHitNormal;
		if (ASavaDomeShield* Shield = ASavaDomeShield::FindBlockingShield(
			Avatar->GetWorld(), ViewLocation, bHit ? Hit.ImpactPoint : TraceEnd, ShieldHitLocation, ShieldHitNormal))
		{
			Hit = FHitResult(Shield, Cast<UPrimitiveComponent>(Shield->GetRootComponent()), ShieldHitLocation, ShieldHitNormal);
			Hit.TraceStart = ViewLocation;
			Hit.TraceEnd = TraceEnd;
			Hit.Distance = FVector::Dist(ViewLocation, ShieldHitLocation);
			OutHits.Add(Hit);
			continue;
		}

		if (!bHit)
		{
			//外れ: 弾道の演出用に、レイの端を当たった位置として入れておく
			Hit = FHitResult(ViewLocation, TraceEnd);
			Hit.Location = TraceEnd;
			Hit.ImpactPoint = TraceEnd;
		}
		else if (IsHeadHit(Hit))
		{
			Hit.BoneName = HeadBoneName;
		}
		OutHits.Add(Hit);
	}
}

bool USavaWeaponFireAbility::IsHeadHit(const FHitResult& Hit) const
{
	const ACharacter* HitCharacter = Cast<ACharacter>(Hit.GetActor());
	if (!HitCharacter)
	{
		return false;
	}

	//カプセルの上端から Headshot Height までを頭とみなす
	const UCapsuleComponent* Capsule = HitCharacter->GetCapsuleComponent();
	const float TopZ = Capsule->GetComponentLocation().Z + Capsule->GetScaledCapsuleHalfHeight();
	return Hit.ImpactPoint.Z >= TopZ - HeadshotHeight;
}
